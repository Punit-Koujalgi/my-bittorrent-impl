
#include "downloader.h"
#include "network_helper.h"

#include <thread>
#include <mutex>
#include <algorithm>
#include <queue>
#include <fstream>
#include <assert.h>

#define BLOCK_SIZE_FOR_PIECE (16 * 1024)
#define REQUEST_PIPELINE_LEN 5

namespace Downloader
{
	std::vector<std::thread> thread_pool;
	std::queue<Piece_Info> pieces_queue;
	std::mutex queue_mutex;

	class piece_comparator
	{
		public:
			bool operator()(const Piece_Info& first, const Piece_Info& second)
			{
				return first.piece_index > second.piece_index; // swaps if false
			}
	};

	std::priority_queue<Piece_Info, std::vector<Piece_Info>, piece_comparator> downloaded_piece_pq;
	std::mutex pq_mutex;

	int start_downloader(Torrent::TorrentData &torrent_data, int piece_index)
	{
		populate_work_queue(torrent_data, piece_index);

		// determine thread pool size (each thread is a connection to a peer)
		// pool_size = min(peers, pieces, threshold) else 1 if piece_index is provided
		int pool_size = 0;

		if (piece_index >= 0)
		{
			pool_size = 1; // downloading a single piece
		}
		else
		{
			pool_size = std::min(torrent_data.peers.size(), torrent_data.piece_hashes.size());
			int pool_threshold = 10;

			if (pool_size > pool_threshold)
				pool_size = pool_threshold;
		}

		auto start = std::chrono::high_resolution_clock::now();
		
		if (initialize_thread_pool(pool_size, torrent_data) != 0)
		{
			std::cerr << "Failed to initialize thread pool" << std::endl;
			return -1;
		}

		wait_for_download(torrent_data);

		auto stop = std::chrono::high_resolution_clock::now();
		std::cout << "Time taken for download: " << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count() << " ms\n";

		return 0;
	}

	int initialize_thread_pool(int pool_size, Torrent::TorrentData &torrent_data)
	{
		std::cout << "Creating " << pool_size << " threads in the pool..." << std::endl;

		for (int peer_index = 0; peer_index < pool_size; ++peer_index)
			thread_pool.emplace_back(thread_function, &torrent_data, peer_index);

		return 0;
	}

	void thread_function(Torrent::TorrentData* torrent_data, int peer_index)
	{
		while (true)
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			
			if (pieces_queue.empty())
				break;

			auto piece_info = std::move(pieces_queue.front());
			pieces_queue.pop();

			lock.unlock();

			if (piece_info.downloaded_len < piece_info.piece_len)
			{
				try
				{
					download_piece(torrent_data, piece_info, peer_index);

					std::unique_lock<std::mutex> lock(pq_mutex);
					downloaded_piece_pq.push(std::move(piece_info));
				}
				catch (const std::exception& e)
				{
					std::cerr << "Failed to download piece " << piece_info.piece_index << ". Err: " << e.what() << "\n";

					std::unique_lock<std::mutex> lock(queue_mutex);
					pieces_queue.push(piece_info);
					lock.unlock();
				}
			}
		}

		std::cout << "Thread #" << peer_index << " exiting...\n";
	}

	int wait_for_download(const Torrent::TorrentData &torrent_data)
	{

		for (auto& thread : thread_pool)
			thread.join();

		// piece data together and write to file
		std::ofstream output_file(torrent_data.out_file, std::ios::binary | std::ios::out);
		if (!output_file.is_open())
		{
			std::cerr << "Failed to open output file: " << torrent_data.out_file << "\n";
			return -1;
		}
		
		while (!downloaded_piece_pq.empty())
		{
			auto piece = std::move(downloaded_piece_pq.top());
			downloaded_piece_pq.pop();

			// Read piece file and write to actual file
			std::ifstream piece_file(piece.piece_file, std::ios::binary | std::ios::in);
			if (!piece_file.is_open())
			{
				std::cerr << "Failed to open output file: " << piece.piece_file << "\n";
				output_file.close();
				return -1;
			}

			output_file << piece_file.rdbuf();

			piece_file.close();
			std::remove(piece.piece_file.c_str());
		}

		output_file.close();
		return 0;
	}

	void populate_work_queue(const Torrent::TorrentData &torrent_data, int piece_index)
	{
		int num_of_pieces = torrent_data.piece_hashes.size();
		int piece_len = torrent_data.piece_length;
		int curr_total_len = torrent_data.length;

		for (int curr_piece_index = 0; curr_piece_index < num_of_pieces; ++curr_piece_index)
		{
			Piece_Info piece;

			piece.piece_index = curr_piece_index;
			piece.piece_len = curr_total_len < piece_len ? curr_total_len : piece_len;
			piece.piece_hash = torrent_data.piece_hashes[curr_piece_index];
			curr_total_len -= piece.piece_len;

			if (piece_index < 0 || curr_piece_index == piece_index)
				pieces_queue.push(std::move(piece));
		}

		assert(curr_total_len == 0);
		std::cout << "Populated pieces work queue. Size: " << pieces_queue.size() << std::endl;
	}

	void download_piece(Torrent::TorrentData *torrent_data, Piece_Info &piece, int peer_index)
	{
		piece.piece_data.clear(); // clear any previous half downloaded piece data
		piece.downloaded_len = 0;
		piece.piece_data.resize(piece.piece_len, '\0');

		std::cout << "Downloading piece: " << piece.piece_index << " in Thread #" << peer_index << "\n";

		Network::Peer& peer = torrent_data->peers[peer_index];
		if (peer.peer_socket == 0)
		{
			if (Network::receive_peer_id_with_handshake(*torrent_data, peer.value(), peer) != 0)
				throw std::runtime_error("Failed to connect to peer");

			// receive and skip bitfield
			handle_bitfield_msg(peer.peer_socket);

			// send interested and receive unchoke msg
			handle_unchoke_msg(peer.peer_socket);
		}

		// send request messages
		handle_request_msgs(piece, peer);

		if (piece.downloaded_len == piece.piece_len)
			verify_piece_hash(piece, torrent_data->out_file);
	}

	void handle_bitfield_msg(int peer_socket)
	{
		std::vector<Network::Peer_Msg> peer_msgs;
		Network::receive_peer_msgs(peer_socket, peer_msgs, 1);

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

		Network::receive_peer_msgs(peer_socket, peer_msgs, 1);
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
			auto block_length = piece.piece_len - piece.downloaded_len < BLOCK_SIZE_FOR_PIECE ? piece.piece_len - piece.downloaded_len : BLOCK_SIZE_FOR_PIECE;
			auto block_length_bytes = Encoder::uint32_to_uint8(block_length);

			Network::Peer_Msg peer_msg;
			peer_msg.msg_type = message_type::REQUEST;
			peer_msg.payload.insert(peer_msg.payload.end(), index_bytes.begin(), index_bytes.end());
			peer_msg.payload.insert(peer_msg.payload.end(), begin_bytes.begin(), begin_bytes.end());
			peer_msg.payload.insert(peer_msg.payload.end(), block_length_bytes.begin(), block_length_bytes.end());

			peer_msgs.push_back(peer_msg);

			++requests_sent;
			piece.downloaded_len += block_length;

			if ((requests_sent != 0 && requests_sent % 5 == 0) || piece.downloaded_len == piece.piece_len)
			{
				auto expected_responses = peer_msgs.size();
				
				if (Network::send_peer_msgs(peer.peer_socket, peer_msgs) != 0)
					throw std::runtime_error("Failed to send peer msgs");

				handle_piece_msgs(piece, peer, expected_responses);
			}
		}
	}

	void handle_piece_msgs(Piece_Info &piece, Network::Peer &peer, int expected_responses)
	{
		std::vector<Network::Peer_Msg> peer_msgs;

		if (Network::receive_peer_msgs(peer.peer_socket, peer_msgs, expected_responses) != 0)
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

			std::copy(peer_msg.payload.begin() + 8, peer_msg.payload.end(), piece.piece_data.begin() + begin_byte);
			//piece.piece_data.insert(piece.piece_data.begin() + begin_byte, peer_msg.payload.begin() + 8, peer_msg.payload.end());
		}
	}

	void verify_piece_hash(Piece_Info &piece, const std::string& out_file)
	{
		// calculate hash of downloaded piece
		std::string downloaded_data_hash = Encoder::hast_to_hex(Encoder::SHA_string(piece.piece_data));

		if (downloaded_data_hash != piece.piece_hash)
			throw std::runtime_error("Hash of downloaded data doesn't match actual hash: " + downloaded_data_hash + " " + piece.piece_hash);

		std::unique_lock<std::mutex> lock(queue_mutex); // use another mutex for output from threads

		piece.piece_file = out_file + "_piece_" + std::to_string(piece.piece_index);

		std::ofstream piece_file(piece.piece_file);
		piece_file << std::move(piece.piece_data);
		piece_file.close();

		piece.piece_data.clear();
		std::cout << "Piece #" << piece.piece_index << " successfully downloaded!\n";
	}

}

