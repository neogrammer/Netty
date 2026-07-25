#include "Client.h"
#include <network/NetworkCommon.h>
#include <network/NetTypes.h>
#include <entities/Entity.h>
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <SFML/System.hpp>
#include <unordered_map>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <mgmt/GameStateManager.h>
#include <res/Cfg.h>

void run_client(int server_tcp_port) {
    const sf::IpAddress server_ip(24, 35, 13, 61);  // change to real IP

    unsigned short my_udp_port;
    int player_id;
    sf::TcpSocket tcp_socket;
    sf::UdpSocket udp_socket = tcp_handshake_client(server_ip, server_tcp_port,
        &my_udp_port, &player_id, tcp_socket);
    if (udp_socket.getLocalPort() == 0) return;

    udp_socket.setBlocking(false);
    tcp_socket.setBlocking(false);

    const unsigned short server_udp_port = 57913;
    bool tcp_ok_received = false;

    printf("[Client %d] Sending UDP 'OK' and waiting for TCP confirmation...\n", player_id);
    while (!tcp_ok_received) {
        const char* ok_msg = "OK";
        udp_socket.send(ok_msg, std::strlen(ok_msg), server_ip, server_udp_port);
        sf::Packet packet;
        if (tcp_socket.receive(packet) == sf::Socket::Status::Done) {
            std::string confirm;
            packet >> confirm;
            if (confirm == "OK") {
                tcp_ok_received = true;
                printf("[Client %d] Received TCP 'OK' – ready.\n", player_id);
                break;
            }
        }
        sf::sleep(sf::milliseconds(100));
    }
    tcp_socket.setBlocking(false);

    Cfg::Initialize();

    // ---- Animation setup ----  good long as the client is running
    AnimationSet playerAnimSet;
    initPlayerAnimations(playerAnimSet);
    std::unordered_map<EntityType, AnimationSet*> entityAnimSets = {
        { EntityType::Player, &playerAnimSet }
    };

    sf::RenderWindow window(sf::VideoMode({ 800, 600 }), "Game Client");
    window.setFramerateLimit(60);

    // Create game state manager and register states
    GameStateManager gsm;
    gsm.registerState("title", std::make_unique<TitleState>(&window, &gsm));
    gsm.registerState("play", std::make_unique<PlayState>(&window, &udp_socket, &tcp_socket,
        server_ip, server_udp_port, player_id, entityAnimSets));
    gsm.switchTo("title");   // start on title screen

    sf::Clock clock;
    while (window.isOpen()) {
        // Process window events
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();
            else
                gsm.handleEvent(*event);
        }

        // Update & draw current state
        gsm.update(clock.restart());
        gsm.draw(window);
    }
}