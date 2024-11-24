
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

	std::string hast_to_hex(const std::string& hash);

	std::string encode_info_hash(const std::string& hash);

}

namespace Torrent
{
	struct TorrentData
	{
		std::string tracker;
		int length = 0;
		std::string info_hash;
		int piece_length = 0;
		std::vector<std::string> piece_hashes;
	};

	int read_torrent_file(const std::string& torrent_file, TorrentData& torrent_data);
}


#endif




