
#include "magnet_links.h"
#include "bencode_helper.h"

#include <string_view>

namespace Magnet
{
	int parse_magnet_link(const std::string& magnet_link, Torrent::TorrentData& torrent_data)
	{
		std::string_view link_view(magnet_link);
		link_view = link_view.substr(link_view.find(":?") + 2);

		do
		{
			auto curr_key_val = link_view.substr(0, link_view.find('&'));
			auto curr_key = curr_key_val.substr(0, curr_key_val.find('='));
			auto curr_val = curr_key_val.substr(curr_key_val.find('=') + 1);

			if (curr_key == "xt")
			{
				std::string hex_hash(curr_val.substr(std::string("urn:btih:").size()));
				torrent_data.info_hash = std::move(Encoder::hex_to_hash(hex_hash));
			}
			
			if (curr_key == "dn")
				torrent_data.out_file = curr_val;

			if (curr_key == "tr")
			{
				std::string curr_val_str(curr_val);
				curr_val_str.replace(curr_val_str.find("%3A"), 3, ":");
				while (curr_val_str.find("%2F") != std::string::npos)
					curr_val_str.replace(curr_val_str.find("%2F"), 3, "/");

				torrent_data.tracker = curr_val_str;
			}

			if (link_view.find('&') == std::string::npos)
				break;
			link_view = link_view.substr(link_view.find('&') + 1);

		} while (true);
		

		return 0;
	}

}

