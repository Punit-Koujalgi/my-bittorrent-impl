
#include "bencode_decoder.h"

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

}

