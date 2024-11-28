
#include "downloader.h"
#include "network_helper.h"

#include <pthread.h>
#include <algorithm>
#include <queue>

#define BLOCK_SIZE_FOR_PIECE (16 * 1024)
#define REQUEST_PIPELINE_LEN 5

namespace Downloader
{
	std::vector<pthread_t> thread_pool;
	std::queue<Piece_Info> pieces_queue;
	pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

	int start_downloader(const Torrent::TorrentData &torrent_data, int piece_index)
	{
		populate_work_queue(torrent_data);

		// determine thread pool size (each thread is a connection to a peer)
		// pool_size = min(peers, pieces, threshold) else 1 if piece_index is provided
		int pool_size = 0;

		if (piece_index > 0)
		{
			pool_size = 1; // downloading a single piece
		}
		else
		{
			int pool_size = std::min(torrent_data.peers.size(), torrent_data.piece_hashes.size());
			int pool_threshold = 10;

			if (pool_size > pool_threshold)
				pool_size = pool_threshold;
		}

		if (initialize_thread_pool(pool_size, torrent_data) != 0)
		{
			std::cerr << "Failed to initialize thread pool" << std::endl;
			return -1;
		}

		return 0;
	}

	int initialize_thread_pool(int pool_size, const Torrent::TorrentData &torrent_data)
	{
		std::cout << "Creating threads in the pool..." << std::endl;

		for (int i = 0; i < pool_size; ++i)
		{
			pthread_t new_thread;
			thread_arg arg{&torrent_data, i};

			if (pthread_create(&new_thread, NULL, thread_function, (void *)&arg) != 0)
			{
				std::cerr << "Failed to create thread!" << std::endl;
				return -1;
			}

			thread_pool.push_back(new_thread);
		}

		return 0;
	}

	void *thread_function(void *arg)
	{
		auto *torrent_data = ((thread_arg *)arg)->torrent_data;
		auto peer_index = ((thread_arg *)arg)->peer_index; // it's also thread id

		while (true)
		{
			pthread_mutex_lock(&queue_mutex);

			if (pieces_queue.empty())
				break;

			auto piece_info = std::move(pieces_queue.front());
			pieces_queue.pop();

			pthread_mutex_unlock(&queue_mutex);

			if (piece_info.downloaded_len < piece_info.piece_len)
			{
				try
				{
					download_piece(torrent_data, piece_info, peer_index);
				}
				catch (const std::exception& e)
				{
					std::cerr << "Failed to download piece " << piece_info.piece_index << "\n";

					pthread_mutex_lock(&queue_mutex);
					pieces_queue.push(piece_info);
					pthread_mutex_unlock(&queue_mutex);
				}
			}
		}

		std::cout << "Thread exiting...\n";
		return nullptr;
	}

	void populate_work_queue(const Torrent::TorrentData &torrent_data)
	{
		int num_of_pieces = torrent_data.piece_hashes.size();
		int piece_len = torrent_data.piece_length;

		for (int piece_index = 0; piece_index < num_of_pieces; ++piece_index)
		{
			Piece_Info piece;

			piece.piece_index = piece_index;
			piece.piece_len = piece_len > BLOCK_SIZE_FOR_PIECE ? BLOCK_SIZE_FOR_PIECE : piece_len;
			piece.piece_hash = torrent_data.piece_hashes[piece_index];

			pieces_queue.push(std::move(piece));
		}

		std::cout << "Populated pieces work queue. Size: " << pieces_queue.size() << std::endl;
	}

	void download_piece(const Torrent::TorrentData *torrent_data, Piece_Info &piece, int peer_index)
	{
		piece.piece_data.clear(); // clear any previous half downloaded piece data
		piece.downloaded_len = 0;
		piece.piece_data.resize(piece.piece_len, '\0');

		std::cout << "Downloading piece: " << piece.piece_index << " in Thread #" << piece.piece_index << "\n";

		auto peer = torrent_data->peers[peer_index];
		if (Network::receive_peer_id_with_handshake(*torrent_data, peer.value(), peer) != 0)
			throw std::runtime_error("Failed to connect to peer");

		// receive and skip bitfield
		handle_bitfield_msg(peer.peer_socket);

		// send interested and receive unchoke msg
		handle_unchoke_msg(peer.peer_socket);

		// send request messages
		handle_request_msgs(piece, peer);

		if (piece.downloaded_len == piece.piece_len)
			verify_piece_hash(piece);
	}

	void handle_bitfield_msg(int peer_socket)
	{
		std::vector<Network::Peer_Msg> peer_msgs;
		Network::receive_peer_msgs(peer_socket, peer_msgs);

		if (peer_msgs.size() != 1 && peer_msgs[0].msg_type != message_type::BITFIELD)
			throw std::runtime_error("Expected bit field msg but got " + peer_msgs[0].msg_type);
	}

	void handle_unchoke_msg(int peer_socket)
	{
		// Interested msg
		Network::Peer_Msg peer_msg;
		peer_msg.msg_type = message_type::INTERESTED;

		std::vector<Network::Peer_Msg> peer_msgs{peer_msg};
		if (Network::send_peer_msgs(peer_socket, peer_msgs) != 0)
			throw std::runtime_error("Error when sending interested msg");

		Network::receive_peer_msgs(peer_socket, peer_msgs);
		if (peer_msgs.size() != 1 && peer_msgs[0].msg_type != message_type::UNCHOKE)
			throw std::runtime_error("Error when receiving unchoke"); // try loop instead?
	}

	void handle_request_msgs(Piece_Info &piece, Network::Peer &peer)
	{
		int requests_sent = 0;
		std::vector<Network::Peer_Msg> peer_msgs;

		while (piece.downloaded_len < piece.piece_len)
		{
			// construct payload for the msg
			auto index_bytes = Encoder::uint32_to_uint8(piece.piece_index);
			auto begin_bytes = Encoder::uint32_to_uint8(requests_sent * BLOCK_SIZE_FOR_PIECE);
			auto length_bytes = Encoder::uint32_to_uint8(piece.piece_len - piece.downloaded_len < BLOCK_SIZE_FOR_PIECE ? piece.piece_len - piece.downloaded_len : BLOCK_SIZE_FOR_PIECE);

			Network::Peer_Msg peer_msg;
			peer_msg.msg_type = message_type::REQUEST;
			peer_msg.payload.insert(peer_msg.payload.end(), index_bytes.begin(), index_bytes.end());
			peer_msg.payload.insert(peer_msg.payload.end(), begin_bytes.begin(), begin_bytes.end());
			peer_msg.payload.insert(peer_msg.payload.end(), length_bytes.begin(), length_bytes.end());

			peer_msgs.push_back(peer_msg);

			if (requests_sent != 0 && requests_sent % 5 == 0)
			{
				if (Network::send_peer_msgs(peer.peer_socket, peer_msgs) != 0)
					throw std::runtime_error("Failed to send peer msgs");
				handle_piece_msgs(piece, peer);
			}
		}
	}

	void handle_piece_msgs(Piece_Info &piece, Network::Peer &peer)
	{
		std::vector<Network::Peer_Msg> peer_msgs;

		if (Network::receive_peer_msgs(peer.peer_socket, peer_msgs) != 0)
			throw std::runtime_error("Failed to receive peer msgs");

		// process piece responses
		for (auto &peer_msg : peer_msgs)
		{
			if (peer_msg.msg_type != message_type::PIECE)
				throw std::runtime_error("Expected piece msg but received " + peer_msg.msg_type);

			if (peer_msg.payload.size() < 8)
				throw std::runtime_error("piece msg with incorrect length received");

			uint32_t piece_index = Encoder::uint8_to_uint32(peer_msg.payload[0], peer_msg.payload[1], peer_msg.payload[2], peer_msg.payload[3]);

			if (piece_index != piece.piece_index)
			{
				throw std::runtime_error("Expected piece not same as received index" +
										 piece.piece_index + std::string(" ") + std::to_string(piece_index));
			}

			uint32_t begin_byte = Encoder::uint8_to_uint32(peer_msg.payload[4], peer_msg.payload[5], peer_msg.payload[6], peer_msg.payload[7]);

			piece.piece_data.insert(piece.piece_data.begin() + begin_byte, peer_msg.payload.begin() + 8, peer_msg.payload.end());
		}
	}

	void verify_piece_hash(Piece_Info &piece)
	{
		// calculate hash of downloaded piece
		std::string downloaded_data_hash = Encoder::hast_to_hex(Encoder::SHA_string(piece.piece_data));

		if (downloaded_data_hash != piece.piece_hash)
			throw std::runtime_error("Hash of downloaded data doesn't match actual hash: " + downloaded_data_hash + " " + piece.piece_hash);
	}

}
