#include "NetworkCommon.h"
#include <cstdio>
#include <cstring>

bool tcp_handshake_server(sf::TcpSocket out_tcp_sockets[2],
    unsigned short out_client_ports[2],
    unsigned short tcp_port)
{
    sf::TcpListener listener;
    if (listener.listen(tcp_port) != sf::Socket::Status::Done) {
        fprintf(stderr, "[Server] TCP listener failed on port %d\n", tcp_port);
        return false;
    }
    printf("[Server] TCP handshake on port %d\n", listener.getLocalPort());

    for (int i = 0; i < 2; ++i) {
        printf("[Server] Waiting for player %d...\n", i + 1);
        sf::TcpSocket client;
        if (listener.accept(client) != sf::Socket::Status::Done) {
            fprintf(stderr, "[Server] Accept failed for player %d\n", i + 1);
            return false;
        }

        sf::Packet packet;
        int player_id = i;
        unsigned short server_udp_port = 57913;
        packet << player_id << server_udp_port;
        if (client.send(packet) != sf::Socket::Status::Done) {
            fprintf(stderr, "[Server] Send to player %d failed\n", i);
            return false;
        }

        packet.clear();
        if (client.receive(packet) != sf::Socket::Status::Done) {
            fprintf(stderr, "[Server] Receive from player %d failed\n", i);
            return false;
        }
        unsigned short client_udp_port;
        packet >> client_udp_port;
        out_client_ports[i] = client_udp_port;

        out_tcp_sockets[i] = std::move(client);
        printf("[Server] Player %d registered (UDP port %d)\n", i, client_udp_port);
    }
    return true;
}

sf::UdpSocket tcp_handshake_client(const sf::IpAddress& server_ip,
    unsigned short server_tcp_port,
    unsigned short* out_my_udp_port,
    int* out_player_id,
    sf::TcpSocket& out_tcp_socket)
{
    sf::TcpSocket tcp_socket;
    if (tcp_socket.connect(server_ip, server_tcp_port) != sf::Socket::Status::Done) {
        fprintf(stderr, "[Client] TCP connect to %s:%d failed\n",
            server_ip.toString().c_str(), server_tcp_port);
        return sf::UdpSocket();
    }

    sf::Packet packet;
    if (tcp_socket.receive(packet) != sf::Socket::Status::Done) {
        fprintf(stderr, "[Client] TCP receive failed\n");
        return sf::UdpSocket();
    }
    int player_id;
    unsigned short server_udp_port;
    packet >> player_id >> server_udp_port;
    *out_player_id = player_id;

    sf::UdpSocket udp_socket;
    if (udp_socket.bind(sf::Socket::AnyPort) != sf::Socket::Status::Done) {
        fprintf(stderr, "[Client] UDP bind failed\n");
        return sf::UdpSocket();
    }
    *out_my_udp_port = udp_socket.getLocalPort();

    packet.clear();
    packet << *out_my_udp_port;
    if (tcp_socket.send(packet) != sf::Socket::Status::Done) {
        fprintf(stderr, "[Client] Send UDP port failed\n");
        return sf::UdpSocket();
    }

    out_tcp_socket = std::move(tcp_socket);
    return udp_socket;
}