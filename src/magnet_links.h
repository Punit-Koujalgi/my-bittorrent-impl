
#ifndef _MAGNET_LINKS_H_
#define _MAGNET_LINKS_H_

#include <string>

namespace Torrent
{
	struct TorrentData;
}

namespace Network
{
	struct Peer;
	struct Peer_Msg;
}

namespace Magnet
{
	int parse_magnet_link(const std::string& magnet_link, Torrent::TorrentData& torrent_data);

	bool is_extension_supported(const std::string& base_handshake);

	int get_peer_extension_id(Network::Peer& peer);

	int send_and_receive_peer_msg(int peer_socket, const std::string payload, Network::Peer_Msg& peer_msg);

	int receive_torrent_info(const Network::Peer& peer, Torrent::TorrentData& torrent_data, bool do_unchoke);
}


#endif


