
#include "bencode_helper.h"

#include <fstream>

namespace Decoder
{
	json decode_bencoded_value(const std::string &encoded_value)
	{
		size_t position = 0;

		try
		{
			return decode_bencoded_value(encoded_value, position);
		}
		catch(const std::exception& e)
		{
			std::cerr << "Failed to decode string: [" << encoded_value << "]\n";
		}

		return json();
	}

	json decode_bencoded_value(const std::string &encoded_value, size_t& position)
	{
		if (std::isdigit(encoded_value[position]))
		{
			return decode_bencoded_string(encoded_value, position);
		}
		else if(encoded_value[position] == 'i')
		{
			return decode_bencoded_int(encoded_value, position);
		}
		else if(encoded_value[position] == 'l')
		{
			return decode_bencoded_list(encoded_value, position);
		}
		else if(encoded_value[position] == 'd')
		{
			return decode_bencoded_dict(encoded_value, position);
		}
		else
		{
			throw std::runtime_error("Invalid encoded value: " + encoded_value);
		}

	}

	json decode_bencoded_string(const std::string &encoded_value, size_t& position)
	{
		if (std::isdigit(encoded_value[position]))
		{
			// Example: "5:hello" -> "hello"
			size_t colon_index = encoded_value.find(':', position);
			if (colon_index != std::string::npos)
			{
				std::string string_len_str = encoded_value.substr(position, colon_index - position);
				int64_t str_len = std::atoll(string_len_str.c_str());
				std::string str = encoded_value.substr(colon_index + 1, str_len);
				position = colon_index + 1 + str_len;

				return json(str);
			}
			else
			{
				throw std::runtime_error("Invalid encoded value: " + encoded_value);
			}
		}
		else
		{
			throw std::runtime_error("Unhandled encoded value: " + encoded_value);
		}
	}

	json decode_bencoded_int(const std::string &encoded_value, size_t& position)
	{
		if (encoded_value[position] == 'i')
		{
			++position; // 'i'
			auto e_pos = encoded_value.find('e', position);
			std::string number_string = encoded_value.substr(position, e_pos - position);
			int64_t number = std::atoll(number_string.c_str());
			position = e_pos + 1;

			return json(number);
		}
		else
		{
			throw std::runtime_error("Unhandled encoded value: " + encoded_value);
		}
	}

	json decode_bencoded_list(const std::string &encoded_value, size_t& position)
	{
		++position; // 'l'

		json list = json::array();
		while (encoded_value[position] != 'e')
			list.push_back(decode_bencoded_value(encoded_value, position));

		++position; // 'e'
		return list;
	}

	json decode_bencoded_dict(const std::string &encoded_value, size_t& position)
	{
		++position; // 'd'

		json obj = json::object();
		while (encoded_value[position] != 'e')
		{
			json key = decode_bencoded_value(encoded_value, position);
			json value = decode_bencoded_value(encoded_value, position);

			obj[key] = value;
		}

		++position; // 'e'
		return obj;
	}

	std::vector<std::string> get_pieces_list_from_json(const json& j)
	{
		auto hashes_str = j.get<std::string>();
		std::string_view hashes_str_view (hashes_str);
		std::vector<std::string> hash_list;
		
		while (hashes_str_view.length() > 0)
		{
			auto piece_hash = hashes_str_view.substr(0, 20);
			hashes_str_view = hashes_str_view.substr(20);

			std::stringstream ss;

			for (unsigned char byte : piece_hash)
				ss << std::hex << std::setw(2) << std::setfill('0') <<static_cast<int>(byte);

			hash_list.push_back(std::move(ss.str()));
		}

		return hash_list;
	}

}

namespace Encoder
{
	std::string json_to_bencode(const json& j)
	{
		std::ostringstream os;

		if (j.is_object())
		{
			os << 'd';

			for (auto& kv : j.items())
				os << kv.key().size() << ':' << kv.key() << json_to_bencode(kv.value());

			os << 'e';
		}
		else if (j.is_array())
		{
			os << 'l';

			for (auto& item : j)
				os << json_to_bencode(item);

			os << 'e';
		}
		else if (j.is_number_integer())
		{
			os << 'i' << j.get<int>() << 'e';
		}
		else if (j.is_string())
		{
			const auto& str = j.get<std::string>();
			os << str.size() << ':' << str;
		}

		return os.str();
	}

	std::string SHA_string(const std::string& data)
	{
		std::string hash(20, '\0');
		SHA1((unsigned char*)data.c_str(), data.size(), (unsigned char*)hash.data());

		return hash;
	}

	std::string hast_to_hex(const std::string& hash)
	{
		std::stringstream ss;
		ss << std::hex << std::setfill('0');

		for (unsigned char byte : hash)
			ss << std::setw(2) << static_cast<int>(byte);

		return ss.str();
	}

	std::string encode_info_hash(const std::string& hash)
	{
		std::string encoded;
		for(auto i = 0; i < hash.size(); i += 2)
			encoded += '%' + hash.substr(i, 2);
		return encoded;
	}


}

namespace Torrent
{
	int read_torrent_file(const std::string& torrent_file, TorrentData& torrent_data)
	{
		std::ifstream file(torrent_file);
		std::ostringstream sstr;
		
		/* Note:
			std::string can store any sequence of bytes, including those representing valid or invalid UTF-8 characters
			How the string is interpreted depends on the context. Some functions might assume UTF-8 encoding, while others might treat it as raw bytes
			If you need to ensure that a string contains valid UTF-8, you'll need to use additional functions or libraries
		*/

		sstr << file.rdbuf();
		json decoded_data = Decoder::decode_bencoded_value(sstr.str());

		std::string bencoded_info = Encoder::json_to_bencode(decoded_data["info"]);
		std::string info_hash = (bencoded_info);

		torrent_data.tracker = decoded_data["announce"].get<std::string>();
		torrent_data.length = decoded_data["info"]["length"].get<int>();
		torrent_data.info_hash = std::move(Encoder::SHA_string(info_hash));
		torrent_data.piece_length = decoded_data["info"]["piece length"].get<int>();
		torrent_data.piece_hashes = std::move(Decoder::get_pieces_list_from_json(decoded_data["info"]["pieces"]));

		return 0;
	}
}