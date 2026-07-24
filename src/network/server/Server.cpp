#include "Server.h"
#include <network/NetworkCommon.h>
#include <NetTypes.h>
#include <SFML/Network.hpp>
#include <SFML/System.hpp>
#include <vector>
#include <cstdio>
#include <cstring>
#include <algorithm>

static void udp_game_server(sf::TcpSocket tcp_sockets[2],
    const unsigned short client_ports[2])
{
    sf::UdpSocket udp_socket;
    if (udp_socket.bind(57913) != sf::Socket::Status::Done) {
        fprintf(stderr, "[Server] UDP bind failed on port 57913\n");
        return;
    }
    udp_socket.setBlocking(false);

    std::optional<sf::IpAddress> client_ips[2];
    bool client_ok_received[2] = { false, false };
    bool client_confirmed[2] = { false, false };

    printf("[Server] Waiting for UDP 'OK' from both clients...\n");
    while (!client_confirmed[0] || !client_confirmed[1]) {
        char buf[64];
        std::size_t received;
        std::optional<sf::IpAddress> sender_ip;
        unsigned short sender_port;

        while (udp_socket.receive(buf, sizeof(buf) - 1, received,
            sender_ip, sender_port) == sf::Socket::Status::Done) {
            buf[received] = '\0';

            if (std::strcmp(buf, "OK") == 0) {
                for (int i = 0; i < 2; ++i) {
                    if (sender_port == client_ports[i] && !client_ok_received[i]) {
                        client_ips[i] = sender_ip;
                        client_ok_received[i] = true;
                        printf("[Server] Learned endpoint for player %d: %s:%d\n",
                            i, sender_ip->toString().c_str(), sender_port);

                        sf::Packet confirm;
                        std::string ok_str = "OK";
                        confirm << ok_str;
                        if (tcp_sockets[i].send(confirm) == sf::Socket::Status::Done) {
                            client_confirmed[i] = true;
                            printf("[Server] TCP confirmation sent to player %d\n", i);
                        }
                        else {
                            fprintf(stderr, "[Server] Failed to send TCP confirmation to player %d\n", i);
                            client_ok_received[i] = false;
                        }
                        break;
                    }
                }
            }
        }
        sf::sleep(sf::milliseconds(10));
    }

    printf("[Server] Both clients ready. Starting game...\n");

    std::vector<Entity> gameEntities;
    uint32_t nextEntityId = 0;
    uint32_t serverTick = 0;

    auto spawnEntity = [&](float x, float y, uint8_t anim, EntityType etype) -> uint32_t {
        Entity e;
        e.id = nextEntityId++;
        e.x = x; e.y = y;
        e.animation = anim;
        e.animStartTick = serverTick;
        gameEntities.push_back(e);

        SpawnMessage msg;
        msg.entityId = e.id;
        msg.entityType = etype;
        msg.x = x; msg.y = y;
        msg.animation = anim;

        sf::Packet spawnPacket;
        spawnPacket << NetMsgType::SpawnEntity << msg;
        for (int i = 0; i < 2; ++i)
            tcp_sockets[i].send(spawnPacket);

        return e.id;
        };

    uint32_t p0Id = spawnEntity(100.f, 300.f, 0, EntityType::Player);
    uint32_t p1Id = spawnEntity(700.f, 300.f, 0, EntityType::Player);

    const double TICK_DURATION = 1.0 / 60.0;
    const sf::Time TICK_TIME = sf::seconds(1.f / 60.f);
    sf::Clock gameClock;
    sf::Time accumulator = sf::Time::Zero;
    sf::Time previous_time = gameClock.getElapsedTime();

    int p0Facing = 1;   // 1 = right, 0 = left
    int p1Facing = 1;

    while (true) {
        sf::Time dt = gameClock.restart();
        accumulator += dt;

        // Process input
        char buf[64];
        std::size_t received;
        std::optional<sf::IpAddress> senderIp;
        unsigned short senderPort;
        int p0Dir = 0, p1Dir = 0;

        while (udp_socket.receive(buf, sizeof(buf) - 1, received, senderIp, senderPort) == sf::Socket::Status::Done) {
            buf[received] = '\0';
            int playerIdx = (senderPort == client_ports[0]) ? 0 : (senderPort == client_ports[1]) ? 1 : -1;
            if (playerIdx >= 0) {
                if (std::strchr(buf, 'L')) (playerIdx == 0 ? p0Dir : p1Dir) = -1;
                if (std::strchr(buf, 'R')) (playerIdx == 0 ? p0Dir : p1Dir) = 1;
            }
        }

        // Fixed timestep simulation & broadcasting
        while (accumulator >= sf::seconds(static_cast<float>(TICK_DURATION))) {
            accumulator -= sf::seconds(static_cast<float>(TICK_DURATION));
            serverTick++;

            if (p0Dir != 0) p0Facing = (p0Dir > 0) ? 1 : 0;
            if (p1Dir != 0) p1Facing = (p1Dir > 0) ? 1 : 0;

            for (auto& e : gameEntities) {
                if (e.id == p0Id) {
                    e.x += static_cast<float>(p0Dir * 300.0 * TICK_DURATION);
                    uint8_t newAnim = (p0Dir == 0) ? 0 : 1;
                    if (newAnim != e.animation) {
                        e.animation = newAnim;
                        e.animStartTick = static_cast<uint16_t>(serverTick);
                    }
                }
                else if (e.id == p1Id) {
                    e.x += static_cast<float>(p1Dir * 300.0 * TICK_DURATION);
                    uint8_t newAnim = (p1Dir == 0) ? 0 : 1;
                    if (newAnim != e.animation) {
                        e.animation = newAnim;
                        e.animStartTick = static_cast<uint16_t>(serverTick);
                    }
                }
            }

            FrameSnapshot snap;
            snap.frameNumber = serverTick;

            for (const auto& e : gameEntities) {
                EntitySnapshot s;
                s.entityId = e.id;
                s.x_quant = quantise(e.x);
                s.y_quant = quantise(e.y);
                s.animation = e.animation;
                s.animStartTick = e.animStartTick;
                if (e.id == p0Id)
                    s.flags = p0Facing;
                else if (e.id == p1Id)
                    s.flags = p1Facing;
                else
                    s.flags = 0;
                snap.entities.push_back(s);
            }

            sf::Packet snapPacket;
            snapPacket << NetMsgType::FrameSnapshot << snap;
            for (int i = 0; i < 2; ++i)
                udp_socket.send(snapPacket, client_ips[i].value(), client_ports[i]);
        }

        sf::Time next_tick = previous_time + TICK_TIME - sf::microseconds(500);
        sf::Time now = gameClock.getElapsedTime();
        if (next_tick > now)
            sf::sleep(next_tick - now);
    }

    printf("[Server] Finished.\n");
}

void run_server() {
    sf::TcpSocket tcp_sockets[2];
    unsigned short client_udp_ports[2];
    if (!tcp_handshake_server(tcp_sockets, client_udp_ports, 13579)) {
        fprintf(stderr, "Handshake failed.\n");
        return;
    }
    udp_game_server(tcp_sockets, client_udp_ports);
}