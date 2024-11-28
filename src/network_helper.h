
#ifndef _NETWORK_HELPER_H
#define _NETWORK_HELPER_H

#include <string>
#include <vector>
#include <cstdint>

namespace Torrent
{
	struct TorrentData;
}

namespace Network
{
	using UCHAR = unsigned char;

	struct Peer
	{
		std::string peer_id;
		std::string ip_addr;
		std::string port;
		int peer_socket = 0;

		Peer(UCHAR u1, UCHAR u2, UCHAR u3, UCHAR u4, unsigned short port);
		Peer() = default;

		std::string value();
	};

	struct Peer_Msg
	{
		uint64_t total_bytes = 0;
		uint8_t msg_type;
		std::string payload;

		std::string getMessage();
	};

	std::tuple<std::string, std::string> split_domain_and_endpoint(const std::string& tracker_url);

	std::vector<Peer> get_peers(const std::string& info_hash, const std::string& tracker, int length);

	std::vector<Peer> process_peers_str(std::string&& encoded_peers);

	void prepare_handshake(const std::string& hashinfo, std::string& handShake);

	int connect_with_peer(const std::string& peer_addr);

	int receive_peer_id_with_handshake(const Torrent::TorrentData& torrent_data, const std::string& peer_addr_str, Peer& peer);

	int send_peer_msgs(const int peer_socket, std::vector<Peer_Msg>& peer_msgs);

	int receive_peer_msgs(const int peer_socket, std::vector<Peer_Msg>& peer_msgs, int expected_responses);

}

#endif

