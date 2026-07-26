#include "Server.h"
#include <network/NetworkCommon.h>
#include <network/NetTypes.h>
#include <SFML/Network.hpp>
#include <SFML/System.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <game_states/levels/Level.h>
#include <vector>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <csignal>

#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#endif
#include <atomic>



// ---------- Server phase ----------
enum class ServerPhase { Lobby, Playing, GameOver };

// ---------- Global running flag (Ctrl+C) ----------
std::atomic<bool> g_running{ true };

#ifdef _WIN32
BOOL WINAPI consoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        g_running = false;
        return TRUE;
    }
    return FALSE;
}
#else
void signalHandler(int /*signal*/) {
    g_running = false;
}
#endif

// ---------- Camera helper ----------
sf::FloatRect getPlayerCamera(float playerX, float playerY) {
    return sf::FloatRect({ playerX - 800.f, playerY - 450.f }, { 1600.f, 900.f });
}

// ---------- Main server function ----------
static void udp_game_server(sf::TcpSocket tcp_sockets[2],
    const unsigned short client_ports[2])
{

    printf("sizeof(EntitySnapshot) = %zu\n", sizeof(EntitySnapshot));

    // Helpers for sending reliable messages
    auto sendSpawn = [&](int idx, const SpawnMessage& msg) {
        sf::Packet p;
        p << NetMsgType::SpawnEntity << msg;
        tcp_sockets[idx].send(p);
        };
    auto sendDestroy = [&](int idx, const DestroyMessage& msg) {
        sf::Packet p;
        p << NetMsgType::DestroyEntity << msg;
        tcp_sockets[idx].send(p);
        };

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
                        }
                        else {
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

    // ---------- INITIALISE GAME WORLD ----------
    ServerPhase phase = ServerPhase::Lobby;
    Level level;
    //level.allEntities.reserve(32);
    uint32_t nextEntityId = 0;
    uint32_t serverTick = 0;
    uint32_t playerEntityId[2] = { 0xFFFFFFFF, 0xFFFFFFFF };

    auto spawnEntity = [&](float x, float y, uint8_t anim, EntityType etype) -> uint32_t {
        Entity e;
        e.id = nextEntityId++;
        e.type = etype;
        e.x = x; e.y = y;
        e.animation = anim;
        e.animStartTick = serverTick;
        level.addEntity(e);

        SpawnMessage msg;
        msg.entityId = e.id;
        msg.entityType = etype;
        msg.x = x; msg.y = y;
        msg.animation = anim;
        msg.animStartTick = e.animStartTick;
       
        sf::Packet spawnPacket;
        spawnPacket << NetMsgType::SpawnEntity << msg;
        for (int i = 0; i < 2; ++i)
            tcp_sockets[i].send(spawnPacket);
        return e.id;
        };

    // Spawn players
    playerEntityId[0] = spawnEntity(100.f, 300.f, 0, EntityType::Player);
    playerEntityId[1] = spawnEntity(700.f, 300.f, 0, EntityType::Player);
    phase = ServerPhase::Playing;


    


    // Send each player their own entity ID
    for (int i = 0; i < 2; ++i) {
        AssignPlayerMessage assignMsg{ playerEntityId[i] };
        sf::Packet p;
        p << NetMsgType::AssignPlayerEntity << assignMsg;
        tcp_sockets[i].send(p);
    }


    //// after assigning player entities
    //LoadLevelMessage levelMsg{ 1 };   // zone 0
    //for (int i = 0; i < 2; ++i) {
    //    sf::Packet p;
    //    p << NetMsgType::LoadLevel << levelMsg;
    //    tcp_sockets[i].send(p);
    //}

    // ---------- Main loop ----------
    const double TICK_DURATION = 1.0 / 60.0;
    const sf::Time TICK_TIME = sf::seconds(1.f / 60.f);
    sf::Clock gameClock;
    sf::Time accumulator = sf::Time::Zero;
    sf::Time previous_time = gameClock.getElapsedTime();

    int p0Facing = 1, p1Facing = 1;
    std::unordered_set<uint32_t> playerKnownEntities[2];   // per‑client visibility

    playerKnownEntities[0].insert(playerEntityId[0]);
    playerKnownEntities[0].insert(playerEntityId[1]);
    playerKnownEntities[1].insert(playerEntityId[0]);
    playerKnownEntities[1].insert(playerEntityId[1]);

    bool playerReady[2] = { false, false };

    while (g_running) {
        sf::Time dt = gameClock.restart();
        accumulator += dt;

        // ------- Process input -------
        char buf[64];
        std::size_t received;
        std::optional<sf::IpAddress> senderIp;
        unsigned short senderPort;
        int p0Dir = 0, p1Dir = 0;

        while (udp_socket.receive(buf, sizeof(buf) - 1, received,
            senderIp, senderPort) == sf::Socket::Status::Done) {
            buf[received] = '\0';

            // Handle "READY" first
            if (std::strcmp(buf, "READY") == 0) {
                int idx = (senderPort == client_ports[0]) ? 0 :
                    (senderPort == client_ports[1]) ? 1 : -1;
                if (idx >= 0 && !playerReady[idx]) {
                    playerReady[idx] = true;
                    printf("[Server] Player %d is ready\n", idx);

                    // When both are ready, send them into the game
                   // if (playerReady[0] && playerReady[1]) {
                        // Send a LoadLevel (or LoadZone) to both clients
                        LoadLevelMessage levelMsg{ 1 };   // level 1
                        //for (int i = 0; i < 2; ++i) {
                            sf::Packet p;
                            p << NetMsgType::LoadLevel << levelMsg;
                            tcp_sockets[idx].send(p);
                       // }
                        //printf("[Server] Both ready – loading zone 1\n");
                   // }
                }
                continue;   // don't try to parse this as movement input
            }

            // Existing movement input handling (unchanged)
            int playerIdx = (senderPort == client_ports[0]) ? 0 :
                (senderPort == client_ports[1]) ? 1 : -1;
            if (playerIdx >= 0) {
                if (std::strchr(buf, 'L')) (playerIdx == 0 ? p0Dir : p1Dir) = -1;
                if (std::strchr(buf, 'R')) (playerIdx == 0 ? p0Dir : p1Dir) = 1;
            }
        }

        //while (udp_socket.receive(buf, sizeof(buf) - 1, received,
        //    senderIp, senderPort) == sf::Socket::Status::Done) {
        //    buf[received] = '\0';
        //    int playerIdx = (senderPort == client_ports[0]) ? 0 :
        //        (senderPort == client_ports[1]) ? 1 : -1;
        //    if (playerIdx >= 0) {
        //        if (std::strchr(buf, 'L')) (playerIdx == 0 ? p0Dir : p1Dir) = -1;
        //        if (std::strchr(buf, 'R')) (playerIdx == 0 ? p0Dir : p1Dir) = 1;
        //    }
        //}

        // ------- Fixed timestep -------
        while (accumulator >= sf::seconds(static_cast<float>(TICK_DURATION))) {
            accumulator -= sf::seconds(static_cast<float>(TICK_DURATION));
            serverTick++;

            if (phase == ServerPhase::Playing) {
                // Update player facing
                if (p0Dir != 0) p0Facing = (p0Dir > 0) ? 1 : 0;
                if (p1Dir != 0) p1Facing = (p1Dir > 0) ? 1 : 0;

                // Simulate all entities (players + later NPCs etc.)
                for (auto& e : level.allEntities) {
                    if (e.id == playerEntityId[0]) {
                        e.x += static_cast<float>(p0Dir * 300.0 * TICK_DURATION);
                        uint8_t newAnim = (p0Dir == 0) ? 0 : 1;
                        if (newAnim != e.animation) {
                            e.animation = newAnim;
                            e.animStartTick = static_cast<uint32_t>(serverTick);
                        }
                    }
                    else if (e.id == playerEntityId[1]) {
                        e.x += static_cast<float>(p1Dir * 300.0 * TICK_DURATION);
                        uint8_t newAnim = (p1Dir == 0) ? 0 : 1;
                        if (newAnim != e.animation) {
                            e.animation = newAnim;
                            e.animStartTick = static_cast<uint32_t>(serverTick);
                        }
                    }
                }

                // ------- Per‑client visibility & snapshot -------
                for (int i = 0; i < 2; ++i) {
                    Entity* player = level.getEntity(playerEntityId[i]);
                    sf::FloatRect camera = getPlayerCamera(player->x, player->y);

                    auto& known = playerKnownEntities[i];
                    std::vector<uint32_t> toSpawn, toDestroy;

                    // Decide visibility
                    for (auto& e : level.allEntities) {
                        // ---------- Players must always be visible ----------
                        if (e.type == EntityType::Player) {
                            if (!known.count(e.id)) {
                                toSpawn.push_back(e.id);
                                known.insert(e.id);
                            }
                            continue;   // never destroy a player
                        }

                        // ---------- Normal culling for other entities ----------
                        bool visible = camera.contains({ e.x, e.y });
                        bool alreadyKnown = known.count(e.id) > 0;

                        if (visible && !alreadyKnown) {
                            toSpawn.push_back(e.id);
                            known.insert(e.id);
                        }
                        else if (!visible && alreadyKnown) {
                            toDestroy.push_back(e.id);
                            known.erase(e.id);
                        }
                    }

                    // Send spawns (TCP)
                    for (auto id : toSpawn) {
                        Entity& e = *level.getEntity(id);
                        SpawnMessage msg{ e.id, e.type, e.x, e.y, e.animation };
                        sendSpawn(i, msg);
                    }
                    // Send destroys (TCP)
                    for (auto id : toDestroy) {
                        DestroyMessage msg{ id };
                        sendDestroy(i, msg);
                    }

                    // Build snapshot (only known entities)
                    FrameSnapshot snap;
                    snap.frameNumber = serverTick;
                    for (auto id : known) {
                        //Entity& e = *level.entityMap[id];
                        auto it = level.entityIndex.find(id);
                        if (it == level.entityIndex.end()) {
                            // ID is invalid – remove it from the known set and skip
                            known.erase(id);
                            continue;
                        }
                        Entity& e = *level.getEntity(id);
                        EntitySnapshot s;
                        s.entityId = e.id;
                        s.x_quant = quantise(e.x);
                        s.y_quant = quantise(e.y);
                        s.animation = e.animation;
                        s.animStartTick = e.animStartTick;
                        s.flags = (e.id == playerEntityId[0]) ? p0Facing :
                            (e.id == playerEntityId[1]) ? p1Facing : 0;
                        snap.entities.push_back(s);
                    }
                    sf::Packet snapPacket;
                    snapPacket << NetMsgType::FrameSnapshot << snap;
                    udp_socket.send(snapPacket, client_ips[i].value(), client_ports[i]);
                }
            }
        }

        // Frame pacing
        sf::Time next_tick = previous_time + TICK_TIME - sf::microseconds(500);
        sf::Time now = gameClock.getElapsedTime();
        if (next_tick > now)
            sf::sleep(next_tick - now);
    }

    // ---------- Cleanup ----------
    printf("[Server] Shutting down...\n");
    for (int i = 0; i < 2; ++i)
        tcp_sockets[i].disconnect();
    udp_socket.unbind();
    printf("[Server] Finished.\n");
}

void run_server() {
#ifdef _WIN32
    SetConsoleCtrlHandler(consoleHandler, TRUE);
#else
    signal(SIGINT, signalHandler);
#endif

    sf::TcpSocket tcp_sockets[2];
    unsigned short client_udp_ports[2];
    if (!tcp_handshake_server(tcp_sockets, client_udp_ports, 13579)) {
        fprintf(stderr, "Handshake failed.\n");
        return;
    }
    udp_game_server(tcp_sockets, client_udp_ports);
}