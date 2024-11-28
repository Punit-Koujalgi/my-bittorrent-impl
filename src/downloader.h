
#ifndef _DOWNLOADER_H_
#define _DOWNLOADER_H_

#include "bencode_helper.h"

namespace Downloader
{
	struct Piece_Info
	{
		int piece_index = 0;
		int piece_len = 0; // can be different for last piece
		int downloaded_len = 0;
		std::string piece_hash;
		std::string piece_data;
		std::string piece_file;
	};

	enum message_type
	{
		CHOKE = 0,
		UNCHOKE,
		INTERESTED,
		NOT_INTERESTED,
		HAVE,
		BITFIELD,
		REQUEST,
		PIECE,
		CANCEL
	};

	int start_downloader(Torrent::TorrentData& torrent_data, int piece_index = -1); // -1 indicates download all pieces

	int initialize_thread_pool(int pool_size, Torrent::TorrentData &torrent_data);

	int wait_for_download(const Torrent::TorrentData &torrent_data);

	void thread_function(Torrent::TorrentData* torrent_data, int peer_index);

	void populate_work_queue(const Torrent::TorrentData& torrent_data, int piece_index);

	void download_piece(Torrent::TorrentData* torrent_data, Piece_Info& piece_info, int peer_index);

	void handle_bitfield_msg(int peer_socket);

	void handle_unchoke_msg(int peer_socket);

	void handle_request_msgs(Piece_Info& piece, Network::Peer &peer);

	void handle_piece_msgs(Piece_Info& piece, Network::Peer &peer, int expected_responses);

	void verify_piece_hash(Piece_Info& piece, const std::string& out_file);
}

#endif



