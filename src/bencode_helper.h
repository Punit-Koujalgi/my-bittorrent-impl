
#ifndef _BENCODE_HELPER_H_
#define _BENCODE_HELPER_H_

#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <cstdlib>
#include <openssl/sha.h>

#include "lib/nlohmann/json.hpp"

using json = nlohmann::json;

namespace Decoder
{
	json decode_bencoded_value(const std::string &encoded_value);
	
	json decode_bencoded_value(const std::string &encoded_value, size_t& position);

	json decode_bencoded_string(const std::string &encoded_value, size_t& position);

	json decode_bencoded_int(const std::string &encoded_value, size_t& position);

	json decode_bencoded_list(const std::string &encoded_value, size_t& position);

	json decode_bencoded_dict(const std::string &encoded_value, size_t& position);

	std::vector<std::string> get_pieces_list_from_json(const json& j);
}

namespace Encoder
{
	std::string json_to_bencode(const json& j);

	std::string SHA_string(const std::string& data);

	std::string hash_to_hex(const std::string& hash);

	std::string hex_to_hash(const std::string& hex);

	std::string encode_info_hash(const std::string& hash);

	std::vector<uint8_t> uint32_to_uint8(uint32_t value);

	uint32_t uint8_to_uint32(uint8_t a, uint8_t b, uint8_t c, uint8_t d);

}

namespace Network
{
	struct Peer;
}

namespace Torrent
{
	struct FileInfo
	{
		std::vector<std::string> path;  // path components for the file
		int length;                     // length of this specific file
	};

	struct TorrentData
	{
		std::string out_file;
		std::string tracker;
		int length = 0;
		std::string info_hash;
		int piece_length = 0;
		std::vector<std::string> piece_hashes;
		std::vector<Network::Peer> peers;
		bool is_magnet_download = false;
		
		// Multi-file torrent support
		bool is_multi_file = false;
		std::vector<FileInfo> files;  // list of files in multi-file torrent
		std::string name;             // torrent name (directory name for multi-file)
	};

	int read_torrent_file(const std::string& torrent_file, TorrentData& torrent_data);
}


#endif




