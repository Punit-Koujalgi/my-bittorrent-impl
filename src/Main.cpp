#include <iostream>
#include <string>
#include <vector>

#include "bencode_helper.h"
#include "network_helper.h"
#include "downloader.h"
#include "magnet_links.h"

int main(int argc, char *argv[])
{
	// Flush after every std::cout / std::cerr
	std::cout << std::unitbuf;
	std::cerr << std::unitbuf;

	if (argc < 2)
	{
		std::cerr << "Usage: " << argv[0] << " decode <encoded_value>" << std::endl;
		return 1;
	}

	std::string command = argv[1];

	if (command == "decode")
	{
		if (argc < 3)
		{
			std::cerr << "Usage: " << argv[0] << " decode <encoded_value>" << std::endl;
			return 1;
		}

		std::string encoded_value = argv[2];
		json decoded_value = Decoder::decode_bencoded_value(encoded_value);
		std::cout << decoded_value.dump() << std::endl;
	}
	else if (command == "info")
	{
		std::string filename = argv[2];
		Torrent::TorrentData torrent_data;

		if (Torrent::read_torrent_file(filename, torrent_data) != 0)
		{
			std::cerr << "Failed to read torrent file: " << filename << std::endl;
			return 1;
		}

		std::cout << "Tracker URL: " << torrent_data.tracker << std::endl;
		std::cout << "Length: " << torrent_data.length << std::endl;
		std::cout << "Info Hash: " << Encoder::hash_to_hex(torrent_data.info_hash) << std::endl;
		std::cout << "Piece Length: " << torrent_data.piece_length << std::endl;
		std::cout << "Piece Hashes: " << std::endl;

		for (const auto &hash : torrent_data.piece_hashes)
			std::cout << hash << std::endl;
		
		// Display file information
		if (torrent_data.is_multi_file) {
			std::cout << "\nTorrent Type: Multi-file" << std::endl;
			std::cout << "Torrent Name: " << torrent_data.name << std::endl;
			std::cout << "Files (" << torrent_data.files.size() << "):" << std::endl;
			
			for (size_t i = 0; i < torrent_data.files.size(); ++i) {
				const auto& file = torrent_data.files[i];
				std::cout << "  [" << i + 1 << "] ";
				
				// Build full path
				for (size_t j = 0; j < file.path.size(); ++j) {
					if (j > 0) std::cout << "/";
					std::cout << file.path[j];
				}
				
				std::cout << " (" << file.length << " bytes)" << std::endl;
			}
		} else {
			std::cout << "\nTorrent Type: Single-file" << std::endl;
			if (!torrent_data.name.empty()) {
				std::cout << "File Name: " << torrent_data.name << std::endl;
			}
		}
		
		// Display peer information
		std::cout << "\nPeers:" << std::endl;
		if (torrent_data.peers.empty()) {
			std::cout << "  No peers found" << std::endl;
		} else {
			std::cout << "  Found " << torrent_data.peers.size() << " peer(s):" << std::endl;
			for (size_t i = 0; i < torrent_data.peers.size(); ++i) {
				std::cout << "  [" << i + 1 << "] " << torrent_data.peers[i].value() << std::endl;
			}
		}
	}
	else if (command == "peers")
	{
		std::string filename = argv[2];
		Torrent::TorrentData torrent_data;

		if (Torrent::read_torrent_file(filename, torrent_data) != 0)
		{
			std::cerr << "Failed to read torrent file: " << filename << std::endl;
			return 1;
		}

		auto peers = Network::get_peers(torrent_data.info_hash, torrent_data.tracker, torrent_data.length);

		for (auto peer : peers)
			std::cout << peer.value() << std::endl;
	}
	else if (command == "handshake")
	{
		std::string filename = argv[2];
		Torrent::TorrentData torrent_data;

		if (Torrent::read_torrent_file(filename, torrent_data) != 0)
		{
			std::cerr << "Failed to read torrent file: " << filename << std::endl;
			return 1;
		}

		Network::Peer peer;
		if (Network::receive_peer_id_with_handshake(torrent_data, argv[3], peer) != 0)
		{
			std::cerr << "Failed to receive peer id" << std::endl;
			return 1;
		}

		std::cout << "Peer ID: " << Encoder::hash_to_hex(peer.peer_id) << std::endl;
	}
	else if (command == "download_piece" || command == "download")
	{
		if ((command == "download_piece" && argc < 6) || (command == "download" && argc < 5))
		{
			std::cerr << "Insufficient args for the command" << std::endl;
			return 1;
		}

		Torrent::TorrentData torrent_data;

		std::string torrent_file = argv[4];

		int piece_index = -1; // download all pieces
		if (command == "download_piece")
			piece_index = std::stoi(argv[5]);

		if (Torrent::read_torrent_file(torrent_file, torrent_data) != 0)
		{
			std::cerr << "Failed to read torrent file: " << torrent_file << std::endl;
			return 1;
		}

		torrent_data.out_file = argv[3];

		if (Downloader::start_downloader(torrent_data, piece_index) != 0)
		{
			std::cerr << "Failed to download torrent file: " << torrent_file << std::endl;
			return 1;
		}
	}
	else if (command == "magnet_parse")
	{
		Torrent::TorrentData torrent_data;
		Magnet::parse_magnet_link(argv[2], torrent_data);

		std::cout << "Tracker URL: " << torrent_data.tracker << std::endl;
		std::cout << "Info Hash: " << Encoder::hash_to_hex(torrent_data.info_hash) << std::endl;
	}
	else if (command == "magnet_handshake" || command == "magnet_info" || command == "magnet_download_piece" || command == "magnet_download")
	{
		Torrent::TorrentData torrent_data;
		Magnet::parse_magnet_link(command == "magnet_download_piece" || command == "magnet_download" ? argv[4] : argv[2], torrent_data);

		if (torrent_data.peers.empty()) {
			std::cerr << "No peers found for magnet link" << std::endl;
			return 1;
		}

		Network::Peer &peer = torrent_data.peers[0];
		if (Network::receive_peer_id_with_handshake(torrent_data, peer.value(), peer) != 0)
		{
			std::cerr << "Failed to receive peer id" << std::endl;
			return 1;
		}

		if (command == "magnet_handshake")
		{
			std::cout << "Peer ID: " << Encoder::hash_to_hex(peer.peer_id) << std::endl;
			std::cout << "Peer Metadata Extension ID: " << peer.magnet_extension_id << std::endl;
		}
		else
		{
			bool do_unchoke = (command == "magnet_download_piece" || command == "magnet_download");

			if (Magnet::receive_torrent_info(peer, torrent_data, do_unchoke) != 0)
			{
				std::cout << "Failed to receive torrent info from peer\n";
				return 1;
			}

			if (command == "magnet_info")
			{
				std::cout << "Tracker URL: " << torrent_data.tracker << std::endl;
				std::cout << "Length: " << torrent_data.length << std::endl;
				std::cout << "Info Hash: " << Encoder::hash_to_hex(torrent_data.info_hash) << std::endl;
				std::cout << "Piece Length: " << torrent_data.piece_length << std::endl;
				std::cout << "Piece Hashes: " << std::endl;

				for (const auto &hash : torrent_data.piece_hashes)
					std::cout << hash << std::endl;
				
				// Display file information
				if (torrent_data.is_multi_file) {
					std::cout << "\nTorrent Type: Multi-file" << std::endl;
					std::cout << "Torrent Name: " << torrent_data.name << std::endl;
					std::cout << "Files (" << torrent_data.files.size() << "):" << std::endl;
					
					for (size_t i = 0; i < torrent_data.files.size(); ++i) {
						const auto& file = torrent_data.files[i];
						std::cout << "  [" << i + 1 << "] ";
						
						// Build full path
						for (size_t j = 0; j < file.path.size(); ++j) {
							if (j > 0) std::cout << "/";
							std::cout << file.path[j];
						}
						
						std::cout << " (" << file.length << " bytes)" << std::endl;
					}
				} else {
					std::cout << "\nTorrent Type: Single-file" << std::endl;
					if (!torrent_data.name.empty()) {
						std::cout << "File Name: " << torrent_data.name << std::endl;
					}
				}
				
				// Display peer information
				std::cout << "\nPeers:" << std::endl;
				if (torrent_data.peers.empty()) {
					std::cout << "  No peers found" << std::endl;
				} else {
					std::cout << "  Found " << torrent_data.peers.size() << " peer(s):" << std::endl;
					for (size_t i = 0; i < torrent_data.peers.size(); ++i) {
						std::cout << "  [" << i + 1 << "] " << torrent_data.peers[i].value() << std::endl;
					}
				}
			}
			else
			{
				int piece_index = -1; // download all pieces
				if (command == "magnet_download_piece")
					piece_index = std::stoi(argv[5]);

				// Handle -o flag for output file
				if (command == "magnet_download" || command == "magnet_download_piece") {
					if (argc > 3 && std::string(argv[2]) == "-o") {
						torrent_data.out_file = argv[3];
					} else {
						torrent_data.out_file = argv[3]; // fallback
					}
				}

				if (Downloader::start_downloader(torrent_data, piece_index) != 0)
				{
					std::cerr << "Failed to download magnet link!\n";
					return 1;
				}
			}
		}
	}
	else
	{
		std::cerr << "unknown command: " << command << std::endl;
		return 1;
	}

	return 0;
}
