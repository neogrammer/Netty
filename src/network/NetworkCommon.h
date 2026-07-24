#pragma once
#include <SFML/Network.hpp>

// Server‑side handshake: accepts two clients and exchanges UDP ports.
// Returns true on success. Fills out_tcp_sockets and out_client_ports.
bool tcp_handshake_server(sf::TcpSocket out_tcp_sockets[2],
    unsigned short out_client_ports[2],
    unsigned short tcp_port);

// Client‑side handshake: connects, receives player id, binds UDP socket.
// Returns the UDP socket (invalid on failure).
sf::UdpSocket tcp_handshake_client(const sf::IpAddress& server_ip,
    unsigned short server_tcp_port,
    unsigned short* out_my_udp_port,
    int* out_player_id,
    sf::TcpSocket& out_tcp_socket);