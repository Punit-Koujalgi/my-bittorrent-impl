
#include "bencode_decoder.h"

namespace Decoder
{
	json decode_bencoded_value(const std::string &encoded_value)
	{
		if (std::isdigit(encoded_value[0]))
		{
			return decode_bencoded_string(encoded_value);
		}
		else if(encoded_value[0] == 'i')
		{
			return decode_bencoded_int(encoded_value);
		}

		return json();

	}

	json decode_bencoded_string(const std::string &encoded_value)
	{
		if (std::isdigit(encoded_value[0]))
		{
			// Example: "5:hello" -> "hello"
			size_t colon_index = encoded_value.find(':');
			if (colon_index != std::string::npos)
			{
				std::string number_string = encoded_value.substr(0, colon_index);
				int64_t number = std::atoll(number_string.c_str());
				std::string str = encoded_value.substr(colon_index + 1, number);
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

	json decode_bencoded_int(const std::string &encoded_value)
	{
		if (encoded_value[0] == 'i')
		{
			std::string number_string = encoded_value.substr(1, encoded_value.find('e') - 1);
			int64_t number = std::atoll(number_string.c_str());

			return json(number);
		}
		else
		{
			throw std::runtime_error("Unhandled encoded value: " + encoded_value);
		}
	}

}

