
#include "magnet_links.h"
#include "bencode_helper.h"
#include "network_helper.h"
#include "downloader.h"

#include <string_view>

#define MY_PEER_EXTENSION_ID 19

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
		
		torrent_data.is_magnet_download = true;
		torrent_data.peers = Network::get_peers(torrent_data.info_hash, torrent_data.tracker, 999);

		return 0;
	}

	bool is_extension_supported(const std::string& base_handshake)
	{
		std::string_view reserved_bits = base_handshake.substr(20, 8);

		if (static_cast<uint8_t>(reserved_bits[5]) & (1 << 4))
			return true;

		return false;
	}

	int send_and_receive_peer_msg(int peer_socket, const std::string payload, Network::Peer_Msg& peer_msg_resp)
	{
		// Send request to peer
		Network::Peer_Msg peer_msg;

		peer_msg.msg_type = 20;
		peer_msg.payload = std::move(payload);

		std::vector<Network::Peer_Msg> peer_msgs;
		peer_msgs.push_back(peer_msg);

		if (Network::send_peer_msgs(peer_socket, peer_msgs) != 0)
		{
			std::cout << "Failed to send extension handshake msg\n";
			return -1;
		}

		if (Network::receive_peer_msgs(peer_socket, peer_msgs, 1) != 0 || peer_msgs.size() < 1)
		{
			std::cout << "Failed to receive extension handshake msg\n";
			return -1;
		}

		peer_msg_resp = std::move(peer_msgs[0]);

		return 0;
	}

	int get_peer_extension_id(Network::Peer& peer)
	{
		// receive and skip bitfield
		Downloader::handle_bitfield_msg(peer.peer_socket);

		// Send and receive extension handshake
		std::string payload;
		json m_dict = json::object();
		m_dict["m"] = json::object();
		m_dict["m"]["ut_metadata"] = MY_PEER_EXTENSION_ID; // my birthday :)

		payload.push_back(0); // extension handshake
		payload.append(Encoder::json_to_bencode(m_dict));

		Network::Peer_Msg peer_msg;
		if (send_and_receive_peer_msg(peer.peer_socket, payload, peer_msg) != 0)
		{
			std::cout << "Failed to perform handshake\n";
			return -1;
		}

		std::string bencoded_resp = peer_msg.payload.substr(1);
		auto m_dict_resp = Decoder::decode_bencoded_value(bencoded_resp);

		peer.magnet_extension_id = m_dict_resp["m"]["ut_metadata"].get<int>();
		std::cout << "Got peer extension Id: " << peer.magnet_extension_id << "\n";

		return 0;
	}

	int receive_torrent_info(const Network::Peer& peer, Torrent::TorrentData& torrent_data)
	{
		std::string payload;
		json req_dict = json::object();

		req_dict["msg_type"] = 0;
		req_dict["piece"] = 0;

		payload.push_back(static_cast<uint8_t>(peer.magnet_extension_id));
		payload.append(Encoder::json_to_bencode(req_dict));

		Network::Peer_Msg peer_msg;
		if (send_and_receive_peer_msg(peer.peer_socket, payload, peer_msg) != 0
			|| (peer_msg.payload.size() < 1 || static_cast<int>(peer_msg.payload[0]) != MY_PEER_EXTENSION_ID))
		{
			std::cout << "Failed to receive torrent info\n";
			return -1;
		}

		size_t dict_size_offset = 1;
		auto resp_dict = Decoder::decode_bencoded_dict(peer_msg.payload, dict_size_offset);
		std::cout << resp_dict.dump() << " " << dict_size_offset << std::endl;
		if (resp_dict["msg_type"] == 1)
		{
			auto metadata_dict = Decoder::decode_bencoded_value(peer_msg.payload.substr(dict_size_offset, resp_dict["total_size"]));
			
			torrent_data.length = metadata_dict["length"];
			torrent_data.piece_length = metadata_dict["piece length"];
			torrent_data.piece_hashes = std::move(Decoder::get_pieces_list_from_json(metadata_dict["pieces"]));

			return 0;
		}

		return -1;
	}

}

