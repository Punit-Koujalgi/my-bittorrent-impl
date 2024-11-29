
#ifndef _MAGNET_LINKS_H_
#define _MAGNET_LINKS_H_

#include <string>

namespace Torrent
{
	struct TorrentData;
}

namespace Magnet
{
	int parse_magnet_link(const std::string& magnet_link, Torrent::TorrentData& torrent_data);
}


#endif


