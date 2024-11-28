
#include "network_helper.h"
#include "bencode_helper.h"
#include "lib/http/httplib.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BITTORRENT_PROTOCOL "BitTorrent protocol"
#define PEER_ID "PUNITKOUJAPAVANKOUJA"

namespace Network
{
	Peer::Peer(UCHAR u1, UCHAR u2, UCHAR u3, UCHAR u4, unsigned short port)
	{
		this->ip_addr = std::to_string(u1) + '.' + std::to_string(u2) + '.' + std::to_string(u3) + '.' + std::to_string(u4);
		this->port = std::to_string(port);
	}

	std::string Peer::value() { return this->ip_addr + ":" + this->port; }

	std::string Peer_Msg::getMessage()
	{
		std::vector<uint8_t> vec_msg;
		vec_msg.push_back(msg_type);

		if (payload.size() > 0)
			vec_msg.insert(vec_msg.end(), payload.begin(), payload.end());

		auto payload_size_bytes = Encoder::uint32_to_uint8(vec_msg.size());
		vec_msg.insert(vec_msg.begin(), payload_size_bytes.begin(), payload_size_bytes.end());

		std::cout << "DEBUG: total bytes:" << vec_msg.size() << "\n";

		return std::string(vec_msg.begin(), vec_msg.end());
	}

	std::tuple<std::string, std::string> split_domain_and_endpoint(const std::string& tracker_url)
	{
		auto last_forward_slash_index = tracker_url.find_last_of('/');
		auto domain = tracker_url.substr(0, last_forward_slash_index);
		auto endpoint = tracker_url.substr(last_forward_slash_index);
		
		return std::make_tuple(domain, endpoint);
	}

	std::vector<Peer> process_peers_str(std::string&& encoded_peers)
	{
		std::vector<Peer> peers{};

		for (auto i = 0; i < encoded_peers.size(); i += 6)
		{
			auto peer = encoded_peers.substr(i, 6);

			// First four bytes are the ip, the final two bytes are the port in big endian.
			peers.push_back(Peer(static_cast<UCHAR>(peer[0]),
						static_cast<UCHAR>(peer[1]),
						static_cast<UCHAR>(peer[2]),
						static_cast<UCHAR>(peer[3]),
						(static_cast<unsigned short>(peer[4] << 8) | static_cast<unsigned char>(peer[5]))));
		}

		return peers;
	}

	std::vector<Peer> get_peers(const std::string& info_hash, const std::string& tracker, int length)
	{
		httplib::Params params{
			{"peer_id", PEER_ID},
			{"port", "6881"},
			{"uploaded", "0"},
			{"downloaded", "0"},
			{"left", std::to_string(length)},
			{"compact", "1"}
		};

		auto domain_and_endpoint = split_domain_and_endpoint(tracker);
		httplib::Headers headers{};

		auto encoded_info_hash = Encoder::encode_info_hash(Encoder::hast_to_hex(info_hash));

		auto resp = httplib::Client(std::get<0>(domain_and_endpoint))
				// By moving the info hash here we can avoid the url encoding of the query parameter.
				.Get(std::get<1>(domain_and_endpoint) + "?info_hash=" + encoded_info_hash, params, headers);

		auto resp_json = Decoder::decode_bencoded_value(resp->body);//["peers"].get<std::string>();
		
		if (resp_json["failure reason"].empty())
		{
			auto peers_str = resp_json["peers"].get<std::string>();
			return process_peers_str(std::move(peers_str));
		}
		
		std::cerr << "Tracker request failed with err: " << resp_json["failure reason"].get<std::string>() << std::endl;
		return std::vector<Peer>();
		
	}

	void prepare_handshake(const std::string& hashinfo, std::string& handShake)
	{
		char protocolLength = 19;
		handShake.push_back(protocolLength);

		std::string protocol = BITTORRENT_PROTOCOL;
		handShake.insert(handShake.end(), protocol.begin(), protocol.end());
		//eight reserved bytes
		for (int i = 0; i < 8 ; ++i)
			handShake.push_back(0);

		handShake.insert(handShake.end(), hashinfo.begin(), hashinfo.end());
		std::string peerId = PEER_ID;
		handShake.insert(handShake.end(), peerId.begin(), peerId.end());
	}


	int connect_with_peer(const std::string& peer_addr_str)
	{
		std::string peer_ip = peer_addr_str.substr(0, peer_addr_str.find(':'));
		int peer_port = std::stoi(peer_addr_str.substr(peer_addr_str.find(':') + 1));

		int my_socket = socket(AF_INET, SOCK_STREAM, 0);
		if (my_socket < 0)
		{
			std::cerr << "Failed to create socket" << std::endl;
			return -1;
		}

		struct sockaddr_in peer_addr;
		peer_addr.sin_family = AF_INET;
		peer_addr.sin_port = htons(peer_port);

		if (inet_pton(AF_INET, peer_ip.c_str(), &peer_addr.sin_addr) < 0)
		{
			std::cerr << "Invalid IP address" << std::endl;
			return -1;
		}

		if (connect(my_socket, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0)
		{
			std::cerr << "Failed to connect to peer" << std::endl;
			return -1;
		}

		return my_socket;
	}

	int receive_peer_id_with_handshake(const Torrent::TorrentData& torrent_data, const std::string& peer_addr_str, Peer& peer)
	{
		int my_socket = connect_with_peer(peer_addr_str);
		if (my_socket < 0)
		{
			std::cerr << "Failed to connect to peer" << std::endl;
			return -1;
		}

		std::string handshake_msg;
		prepare_handshake(torrent_data.info_hash, handshake_msg);

		if (send(my_socket, handshake_msg.data(), handshake_msg.size(), 0) < 0)
		{
			std::cerr << "Failed to send data to peer" << std::endl;
			return -1;
		}

		std::string handshake_resp(handshake_msg.size(), 0);

		if (recv(my_socket, handshake_resp.data(), handshake_resp.size(), 0) < 0)
		{
			std::cerr << "Failed to receive data from peer" << std::endl;
			return -1;
		}

		if (!handshake_resp.empty())
		{
			peer.peer_id.assign(handshake_resp.end() - 20, handshake_resp.end());			
			peer.peer_socket = my_socket;

			std::cout << "Successfully connected to peer: " << peer_addr_str << "\n";

			return 0;
		}

		return -1;
	}

	int send_peer_msgs(const int peer_socket, std::vector<Peer_Msg>& peer_msgs)
	{
		for (auto& peer_msg : peer_msgs)
		{
			std::string msg_to_send = peer_msg.getMessage();

			if (send(peer_socket, msg_to_send.data(), msg_to_send.size(), 0) < 0)
			{
				std::cerr << "Failed to send data to peer" << std::endl;
				return -1;
			}
		}

		std::cout << "Sent [" << peer_msgs.size() << "] messages to peer\n";
		peer_msgs.clear();

		return 0;
	}

	int receive_peer_msgs(const int peer_socket, std::vector<Peer_Msg>& peer_msgs, int expected_responses)
	{
		peer_msgs.clear();
		int received_responses = 0;

		while (received_responses < expected_responses)
		{
			Peer_Msg peer_msg;

			// receive total length bytes -> 4
			std::string total_len_bytes(4, 0);
			if (recv(peer_socket, total_len_bytes.data(), total_len_bytes.size(), 0) < 0)
			{
				std::cerr << "Incorrect response received\n";
				break; // so we can return with whatever msgs we received correctly
			}

			auto total_len = Encoder::uint8_to_uint32(total_len_bytes[0], total_len_bytes[1], total_len_bytes[2], total_len_bytes[3]);

			// receive rest of the msg
			std::string actual_msg(total_len, 0);
			if (total_len > 1)
			{
				if (recv(peer_socket, actual_msg.data(), actual_msg.size(), 0) != actual_msg.size())
				{
					std::cerr << "Incorrect response received\n";
					break; // so we can return with whatever msgs we received correctly
				}
				peer_msg.payload = actual_msg.substr(1);
			}

			peer_msg.total_bytes = total_len;
			peer_msg.msg_type = actual_msg[0];

			peer_msgs.push_back(peer_msg);
			++received_responses;
		}

		std::cout << "Received [" << peer_msgs.size() << "] messages from peer\n";
		return 0;
	}
}


