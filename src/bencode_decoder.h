
#ifndef _BENCODE_DECODER_H_
#define _BENCODE_DECODER_H_

#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <cstdlib>

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
}


#endif




