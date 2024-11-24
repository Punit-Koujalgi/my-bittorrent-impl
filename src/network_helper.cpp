
#include "network_helper.h"
#include "lib/http/httplib.h"

namespace Network
{
	Peer::Peer(UCHAR u1, UCHAR u2, UCHAR u3, UCHAR u4, int port)
	{
		this->ip_addr = std::to_string(u1) + '.' + std::to_string(u2) + '.' + std::to_string(u3) + '.' + std::to_string(u4);
		this->port = std::to_string(port);
	}

	std::string Peer::value() { return this->ip_addr + ":" + this->port; }

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
						(static_cast<unsigned short>(peer[4] << 8)) | static_cast<unsigned short>(peer[5])));
		}

		return peers;
	}

	std::vector<Peer> get_peers(const Torrent::TorrentData& torrent_data)
	{
		httplib::Params params{
			{"peer_id", "00112233445566778899"},
			{"port", "6881"},
			{"uploaded", "0"},
			{"downloaded", "0"},
			{"left", std::to_string(torrent_data.length)},
			{"compact", "1"}
		};

		auto domain_and_endpoint = split_domain_and_endpoint(torrent_data.tracker);
		httplib::Headers headers{};

		auto encoded_info_hash = Encoder::encode_info_hash(Encoder::hast_to_hex(torrent_data.info_hash));

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
}


