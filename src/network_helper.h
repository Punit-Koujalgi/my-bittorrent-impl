
#ifndef _NETWORK_HELPER_H
#define _NETWORK_HELPER_H

#include "bencode_helper.h"


namespace Network
{
	using UCHAR = unsigned char;

	struct Peer
	{
		std::string ip_addr;
		std::string port;

		Peer(UCHAR u1, UCHAR u2, UCHAR u3, UCHAR u4, unsigned short port);
		std::string value();
	};

	std::tuple<std::string, std::string> split_domain_and_endpoint(const std::string& tracker_url);

	std::vector<Peer> get_peers(const Torrent::TorrentData& torrent_data);

	std::vector<Peer> process_peers_str(std::string&& encoded_peers);

	void prepare_handshake(const std::string& hashinfo, std::string& handShake);

	int connect_with_peer(const std::string& peer_addr);

	int receive_peer_id_with_handshake(const Torrent::TorrentData& torrent_data, const std::string& peer_addr_str, std::string& peer_id);

}

#endif

