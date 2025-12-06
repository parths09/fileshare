#pragma once
#include <atomic>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <set>
extern std::atomic<bool> cancel_requested;
extern bool receiver_active;
using asio::ip::tcp;
struct Peer {
    std::string ip, hostname;
    uint16_t tcp_port;
};

const uint16_t UDP_PORT = 5353;
const uint16_t TCP_PORT = 12345;

std::string get_hostname()
{
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    }
    return "UnknownHost";
}

std::vector<Peer> discover_peers(asio::io_context& io, int timeout_ms = 10000) {
    asio::ip::udp::socket sock(io, asio::ip::udp::v4());
    sock.set_option(asio::socket_base::reuse_address(true));

    sock.bind({asio::ip::address_v4::any(), UDP_PORT});
    
    std::vector<Peer> peers;
    std::set<std::string> seen;  // Dedupe
    
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count() < timeout_ms) {
        std::cout<<"\rDiscovering peers...";
        
        asio::ip::udp::endpoint sender_ep;
        std::array<char, 64> buf;
        size_t len = sock.receive_from(asio::buffer(buf), sender_ep);
        
        if (len >= 44 && std::strncmp(buf.data(), "FSHR", 4) == 0) {
            uint32_t port;
            std::memcpy(&port, buf.data()+4, 4);
            std::string key = sender_ep.address().to_string() + ":" + std::to_string(ntohl(port));
            if (seen.insert(key).second) {
                Peer new_peer;
                new_peer.ip = sender_ep.address().to_string();
                new_peer.tcp_port = ntohl(port);
                new_peer.hostname = std::string(buf.data()+8, 32);
                peers.push_back(new_peer);
            }
        }
    }
    std::cout<<std::endl;
    return peers;
}
void beacon_thread_func(uint16_t tcp_port, std::string hostname) {
    asio::io_context io;
    asio::ip::udp::socket sock(io);
    
    try {
        sock.open(asio::ip::udp::v4());
        sock.set_option(asio::socket_base::reuse_address(true));
        sock.set_option(asio::socket_base::broadcast(true));
        sock.bind({asio::ip::address_v4::any(), UDP_PORT});
        
        std::array<char, 64> msg = {'F','S','H','R'};
        uint32_t net_port = htonl(tcp_port);
        std::memcpy(msg.data()+4, &net_port, 4);
        std::strncpy(msg.data()+8, hostname.c_str(), 32);
        
        asio::ip::udp::endpoint broadcast(asio::ip::address_v4::broadcast(), UDP_PORT);
        
        while (receiver_active) {  // Exit when false
            sock.send_to(asio::buffer(msg), broadcast);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception& e) {
        std::cerr << "Beacon error: " << e.what() << std::endl;
    }
}
