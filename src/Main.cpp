#include <iostream>
#include <string>
#include <vector>
#include <fstream>

#include "bencode_decoder.h"

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
		std::ifstream file(filename);
		std::ostringstream sstr;
		
		sstr << file.rdbuf();
		json decoded_data = Decoder::decode_bencoded_value(sstr.str());

		std::cout << "Tracker URL: " << decoded_data["announce"].get<std::string>() << std::endl;
		std::cout << "Length: " << decoded_data["info"]["length"].get<int>() << std::endl;
	}
	else
	{
		std::cerr << "unknown command: " << command << std::endl;
		return 1;
	}

	return 0;
}

