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

struct PlayerSlot {
    sf::TcpSocket tcpSocket;
    unsigned short udpPort = 0;
    std::optional<sf::IpAddress> ip;
    bool connected = false;
    bool ready = false;

    // Game state (so we can drop p0Dir/p1Dir etc.)
    int dir = 0;
    int facing = 1;   // 1 = right, 0 = left
    std::unordered_set<uint32_t> knownEntities;
};

// ---------- Camera helper ----------
sf::FloatRect getPlayerCamera(float playerX, float playerY) {
    return sf::FloatRect({ playerX - 800.f, playerY - 450.f }, { 1600.f, 900.f });
}


static void  udp_game_server(sf::TcpListener& listener,
    sf::TcpSocket tcp_sockets[2],
    unsigned short client_ports[2],
    int& playerCount)
{
    printf("sizeof(EntitySnapshot) = %zu\n", sizeof(EntitySnapshot));

    // ---------- Player slots ----------
    PlayerSlot slots[2];
    if (playerCount >= 1) {
        slots[0].tcpSocket = std::move(tcp_sockets[0]);
        slots[0].udpPort = client_ports[0];
        slots[0].connected = true;
    }

    // ---------- World state (must be declared before the lambdas that capture them) ----------
    ServerPhase phase = ServerPhase::Lobby;
    Level level;
    uint32_t nextEntityId = 0;
    uint32_t serverTick = 0;
    uint32_t playerEntityId[2] = { 0xFFFFFFFF, 0xFFFFFFFF };

    // ---------- World reset helper ----------
    auto resetWorld = [&]() {
        phase = ServerPhase::Lobby;
        level.allEntities.clear();
        level.entityIndex.clear();
        nextEntityId = 0;
        serverTick = 0;
        playerEntityId[0] = playerEntityId[1] = 0xFFFFFFFF;
        slots[0].knownEntities.clear();
        slots[1].knownEntities.clear();
        printf("[Server] World reset.\n");
        };

    // ---------- Disconnect helper ----------
    auto disconnectPlayer = [&](int idx) {
        if (!slots[idx].connected) return;
        slots[idx].tcpSocket.disconnect();
        slots[idx].connected = false;
        slots[idx].ready = false;
        slots[idx].ip = std::nullopt;
        slots[idx].dir = 0;
        slots[idx].facing = 1;
        slots[idx].knownEntities.clear();
        playerCount--;
        printf("[Server] Player %d disconnected. %d player(s) remaining.\n", idx, playerCount);

        // If no one is left, reset the whole world
        if (playerCount == 0) {
            resetWorld();
            printf("[Server] All players gone – world reset. Waiting for new connections.\n");
        }
        };

    // ---------- World initialization helper ----------
    auto initializeWorld = [&]() {
        phase = ServerPhase::Playing;
        // Spawn players
        auto spawn = [&](float x, float y, uint8_t anim, EntityType etype) -> uint32_t {
            Entity e;
            e.id = nextEntityId++;
            e.type = etype;
            e.x = x; e.y = y;
            e.animation = anim;
            e.animStartTick = serverTick;
            level.addEntity(e);

            SpawnMessage msg{ e.id, e.type, e.x, e.y, e.animation, e.animStartTick };
            sf::Packet sp;
            sp << NetMsgType::SpawnEntity << msg;
            for (int i = 0; i < 2; ++i)
                if (slots[i].connected)
                    slots[i].tcpSocket.send(sp);
            return e.id;
            };
        playerEntityId[0] = spawn(100.f, 300.f, 0, EntityType::Player);
        playerEntityId[1] = spawn(700.f, 300.f, 0, EntityType::Player);
        // Pre‑seed known sets
        for (int i = 0; i < 2; ++i) {
            slots[i].knownEntities.insert(playerEntityId[0]);
            slots[i].knownEntities.insert(playerEntityId[1]);
        }
        printf("[Server] World initialized.\n");
        };
    // ---------- Helper lambdas (send with disconnect on failure) ----------
    auto sendSpawn = [&](int idx, const SpawnMessage& msg) {
        if (!slots[idx].connected) return;
        sf::Packet p;
        p << NetMsgType::SpawnEntity << msg;
        if (slots[idx].tcpSocket.send(p) != sf::Socket::Status::Done)
            disconnectPlayer(idx);
        };
    auto sendDestroy = [&](int idx, const DestroyMessage& msg) {
        if (!slots[idx].connected) return;
        sf::Packet p;
        p << NetMsgType::DestroyEntity << msg;
        if (slots[idx].tcpSocket.send(p) != sf::Socket::Status::Done)
            disconnectPlayer(idx);
        };

    // ---------- UDP setup ----------
    sf::UdpSocket udp_socket;
    if (udp_socket.bind(57913) != sf::Socket::Status::Done) {
        fprintf(stderr, "[Server] UDP bind failed on port 57913\n");
        return;
    }
    udp_socket.setBlocking(false);

    // ---------- UDP "OK" handshake for already connected players ----------
    printf("[Server] Waiting for UDP 'OK' from connected players...\n");
    while (!slots[0].ip.has_value() && !slots[1].ip.has_value()) {
        char buf[64];
        std::size_t received;
        std::optional<sf::IpAddress> sender_ip;
        unsigned short sender_port;
        while (udp_socket.receive(buf, sizeof(buf) - 1, received, sender_ip, sender_port)
            == sf::Socket::Status::Done) {
            buf[received] = '\0';
            if (std::strcmp(buf, "OK") == 0) {
                for (int i = 0; i < 2; ++i) {
                    if (slots[i].connected && sender_port == slots[i].udpPort && !slots[i].ip.has_value()) {
                        slots[i].ip = sender_ip;
                        printf("[Server] Learned endpoint for player %d: %s:%d\n",
                            i, sender_ip->toString().c_str(), sender_port);
                        sf::Packet confirm;
                        std::string ok = "OK";
                        confirm << ok;
                        if (slots[i].tcpSocket.send(confirm) != sf::Socket::Status::Done)
                            disconnectPlayer(i);
                        break;
                    }
                }
            }
        }
        // Late player during handshake
        if (playerCount < 2) {
            sf::TcpSocket newSocket;
            if (listener.accept(newSocket) == sf::Socket::Status::Done) {
                int freeSlot = -1;
                for (int i = 0; i < 2; ++i) {
                    if (!slots[i].connected) { freeSlot = i; break; }
                }
                if (freeSlot >= 0) {
                    unsigned short newUdpPort;
                    sf::Packet pkt;
                    pkt << freeSlot << static_cast<unsigned short>(57913);
                    if (newSocket.send(pkt) != sf::Socket::Status::Done) continue;
                    pkt.clear();
                    if (newSocket.receive(pkt) != sf::Socket::Status::Done) continue;
                    pkt >> newUdpPort;
                    slots[freeSlot].tcpSocket = std::move(newSocket);
                    slots[freeSlot].tcpSocket.setBlocking(false);
                    slots[freeSlot].udpPort = newUdpPort;
                    slots[freeSlot].connected = true;
                    playerCount++;
                    printf("[Server] Late player %d registered during handshake (UDP %d)\n",
                        freeSlot, newUdpPort);
                    // UDP OK handshake for late player
                    char okBuf[4];
                    std::size_t recvd;
                    std::optional<sf::IpAddress> ip;
                    unsigned short port;
                    while (udp_socket.receive(okBuf, 3, recvd, ip, port) != sf::Socket::Status::Done
                        || std::strncmp(okBuf, "OK", 2) != 0 || port != newUdpPort)
                        sf::sleep(sf::milliseconds(10));
                    slots[freeSlot].ip = ip;
                    printf("[Server] Learned endpoint for late player %d: %s:%d\n",
                        freeSlot, ip->toString().c_str(), port);
                    sf::Packet confirm;
                    std::string okStr = "OK";
                    confirm << okStr;
                    if (slots[freeSlot].tcpSocket.send(confirm) != sf::Socket::Status::Done)
                        disconnectPlayer(freeSlot);
                    // If this is the first player, initialize world
                    if (playerCount == 1 && phase == ServerPhase::Lobby)
                        initializeWorld();
                }
            }
        }
        if (slots[0].ip.has_value() || slots[1].ip.has_value()) break;
        sf::sleep(sf::milliseconds(10));
    }

    // If we still have no world, create it now (first player already had OK)
    if (phase == ServerPhase::Lobby && playerCount >= 1)
        initializeWorld();

    // Send AssignPlayerEntity to connected players
    for (int i = 0; i < 2; ++i) {
        if (slots[i].connected) {
            AssignPlayerMessage assignMsg{ playerEntityId[i] };
            sf::Packet ap;
            ap << NetMsgType::AssignPlayerEntity << assignMsg;
            if (slots[i].tcpSocket.send(ap) != sf::Socket::Status::Done)
                disconnectPlayer(i);
        }
    }

    printf("[Server] At least one player ready. Starting game...\n");

    // ---------- Main loop ----------
    const double TICK_DURATION = 1.0 / 60.0;
    const sf::Time TICK_TIME = sf::seconds(1.f / 60.f);
    sf::Clock gameClock;
    sf::Time accumulator = sf::Time::Zero;
    sf::Time previous_time = gameClock.getElapsedTime();

    while (g_running) {
        sf::Time dt = gameClock.restart();
        accumulator += dt;

        // ====== Disconnect detection (non‑blocking) ======
        for (int i = 0; i < 2; ++i) {
            if (!slots[i].connected) continue;
            char dummy[1];
            std::size_t dummyRecv = 0;
            sf::Socket::Status status = slots[i].tcpSocket.receive(dummy, 0, dummyRecv);
            if (status == sf::Socket::Status::Disconnected) {
                printf("[Server] TCP disconnect detected for player %d\n", i);
                disconnectPlayer(i);
            }
        }

        // ====== Accept late players ======
        if (playerCount < 2) {
            sf::TcpSocket newSocket;
            if (listener.accept(newSocket) == sf::Socket::Status::Done) {
                int freeSlot = -1;
                for (int i = 0; i < 2; ++i)
                    if (!slots[i].connected) { freeSlot = i; break; }
                if (freeSlot >= 0) {
                    unsigned short newUdpPort;
                    sf::Packet pkt;
                    pkt << freeSlot << static_cast<unsigned short>(57913);
                    if (newSocket.send(pkt) != sf::Socket::Status::Done) continue;
                    pkt.clear();
                    if (newSocket.receive(pkt) != sf::Socket::Status::Done) continue;
                    pkt >> newUdpPort;
                    slots[freeSlot].tcpSocket = std::move(newSocket);
                    slots[freeSlot].tcpSocket.setBlocking(false);
                    slots[freeSlot].udpPort = newUdpPort;
                    slots[freeSlot].connected = true;
                    playerCount++;
                    printf("[Server] Late player %d joined (UDP %d)\n", freeSlot, newUdpPort);

                    // UDP OK handshake
                    char okBuf[4];
                    std::size_t recvd;
                    std::optional<sf::IpAddress> ip;
                    unsigned short port;
                    while (udp_socket.receive(okBuf, 3, recvd, ip, port) != sf::Socket::Status::Done
                        || std::strncmp(okBuf, "OK", 2) != 0 || port != newUdpPort)
                        sf::sleep(sf::milliseconds(10));
                    slots[freeSlot].ip = ip;
                    printf("[Server] Learned endpoint for late player %d: %s:%d\n",
                        freeSlot, ip->toString().c_str(), port);
                    sf::Packet confirm;
                    std::string okStr = "OK";
                    confirm << okStr;
                    if (slots[freeSlot].tcpSocket.send(confirm) != sf::Socket::Status::Done) {
                        disconnectPlayer(freeSlot);
                        continue;
                    }
                    // If this is the first player, initialize world
                    if (playerCount == 1 && phase == ServerPhase::Lobby)
                        initializeWorld();

                    // Send assignment and existing entities
                    AssignPlayerMessage assignMsg{ playerEntityId[freeSlot] };
                    sf::Packet ap;
                    ap << NetMsgType::AssignPlayerEntity << assignMsg;
                    if (slots[freeSlot].tcpSocket.send(ap) != sf::Socket::Status::Done) {
                        disconnectPlayer(freeSlot);
                        continue;
                    }
                    for (auto& e : level.allEntities) {
                        SpawnMessage spawnMsg{ e.id, e.type, e.x, e.y, e.animation, e.animStartTick };
                        sf::Packet sp;
                        sp << NetMsgType::SpawnEntity << spawnMsg;
                        if (slots[freeSlot].tcpSocket.send(sp) != sf::Socket::Status::Done) {
                            disconnectPlayer(freeSlot);
                            break;
                        }
                    }
                }
            }
        }

        // ====== Input ======
        char buf[64];
        std::size_t received;
        std::optional<sf::IpAddress> senderIp;
        unsigned short senderPort;

        while (udp_socket.receive(buf, sizeof(buf) - 1, received, senderIp, senderPort)
            == sf::Socket::Status::Done) {
            buf[received] = '\0';
            int playerIdx = (senderPort == slots[0].udpPort) ? 0 :
                (senderPort == slots[1].udpPort) ? 1 : -1;
            if (playerIdx < 0 || !slots[playerIdx].connected) continue;

            if (std::strcmp(buf, "READY") == 0) {
                if (!slots[playerIdx].ready) {
                    slots[playerIdx].ready = true;
                    printf("[Server] Player %d is ready – sending LoadLevel\n", playerIdx);
                    LoadLevelMessage levelMsg{ 1 };
                    sf::Packet p;
                    p << NetMsgType::LoadLevel << levelMsg;
                    if (slots[playerIdx].tcpSocket.send(p) != sf::Socket::Status::Done)
                        disconnectPlayer(playerIdx);
                }
                continue;
            }

            if (std::strchr(buf, 'L')) slots[playerIdx].dir = -1;
            if (std::strchr(buf, 'R')) slots[playerIdx].dir = 1;
        }

        // ====== Fixed timestep ======
        while (accumulator >= sf::seconds(static_cast<float>(TICK_DURATION))) {
            accumulator -= sf::seconds(static_cast<float>(TICK_DURATION));
            serverTick++;

            if (phase == ServerPhase::Playing) {
                for (int i = 0; i < 2; ++i)
                    if (slots[i].dir != 0)
                        slots[i].facing = (slots[i].dir > 0) ? 1 : 0;

                for (auto& e : level.allEntities) {
                    if (e.id == playerEntityId[0]) {
                        e.x += static_cast<float>(slots[0].dir * 300.0 * TICK_DURATION);
                        uint8_t newAnim = (slots[0].dir == 0) ? 0 : 1;
                        if (newAnim != e.animation) {
                            e.animation = newAnim;
                            e.animStartTick = static_cast<uint32_t>(serverTick);
                        }
                        slots[0].dir = 0;
                    }
                    else if (e.id == playerEntityId[1]) {
                        e.x += static_cast<float>(slots[1].dir * 300.0 * TICK_DURATION);
                        uint8_t newAnim = (slots[1].dir == 0) ? 0 : 1;
                        if (newAnim != e.animation) {
                            e.animation = newAnim;
                            e.animStartTick = static_cast<uint32_t>(serverTick);
                        }
                        slots[1].dir = 0;
                    }
                }

                for (int i = 0; i < 2; ++i) {
                    if (!slots[i].connected || !slots[i].ip.has_value()) continue;

                    Entity* player = level.getEntity(playerEntityId[i]);
                    if (!player) continue;
                    sf::FloatRect camera = getPlayerCamera(player->x, player->y);

                    auto& known = slots[i].knownEntities;
                    std::vector<uint32_t> toSpawn, toDestroy;

                    for (auto& e : level.allEntities) {
                        if (e.type == EntityType::Player) {
                            if (!known.count(e.id)) {
                                toSpawn.push_back(e.id);
                                known.insert(e.id);
                            }
                            continue;
                        }
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

                    for (auto id : toSpawn) {
                        Entity* ent = level.getEntity(id);
                        if (ent)
                            sendSpawn(i, { ent->id, ent->type, ent->x, ent->y, ent->animation, ent->animStartTick });
                    }
                    for (auto id : toDestroy)
                        sendDestroy(i, { id });

                    FrameSnapshot snap;
                    snap.frameNumber = serverTick;
                    for (auto id : known) {
                        auto it = level.entityIndex.find(id);
                        if (it == level.entityIndex.end()) { known.erase(id); continue; }
                        Entity& e = *level.getEntity(id);
                        EntitySnapshot s;
                        s.entityId = e.id;
                        s.x_quant = quantise(e.x);
                        s.y_quant = quantise(e.y);
                        s.animation = e.animation;
                        s.animStartTick = e.animStartTick;
                        s.flags = (e.id == playerEntityId[0]) ? slots[0].facing :
                            (e.id == playerEntityId[1]) ? slots[1].facing : 0;
                        snap.entities.push_back(s);
                    }
                    sf::Packet snapPacket;
                    snapPacket << NetMsgType::FrameSnapshot << snap;
                    if (slots[i].ip.has_value()) {
                        if (udp_socket.send(snapPacket, slots[i].ip.value(), slots[i].udpPort) != sf::Socket::Status::Done) {
                            // UDP send failure doesn't mean disconnection, but we could log it
                        }
                    }
                }
            }
        }

        // Frame pacing
        sf::Time next_tick = previous_time + TICK_TIME - sf::microseconds(500);
        sf::Time now = gameClock.getElapsedTime();
        if (next_tick > now)
            sf::sleep(next_tick - now);
    }

    printf("[Server] Shutting down...\n");
    for (int i = 0; i < 2; ++i)
        if (slots[i].connected)
            slots[i].tcpSocket.disconnect();
    udp_socket.unbind();
    printf("[Server] Finished.\n");
}

void run_server() {
#ifdef _WIN32
    SetConsoleCtrlHandler(consoleHandler, TRUE);
#else
    signal(SIGINT, signalHandler);
#endif

    sf::TcpListener listener;
    if (listener.listen(13579) != sf::Socket::Status::Done) {
        fprintf(stderr, "[Server] TCP listener failed on port 13579\n");
        return;
    }
    printf("[Server] TCP handshake on port 13579\n");

    sf::TcpSocket tcp_sockets[2];
    unsigned short client_ports[2];
    int playerCount = 0;

    // Wait for the first player (blocking)
    if (!tcp_accept_player(listener, tcp_sockets[0], client_ports[0], 0)) {
        fprintf(stderr, "[Server] First player failed to connect.\n");
        return;
    }
    playerCount = 1;

    // Now make the listener non‑blocking for late joins
    listener.setBlocking(false);
	tcp_sockets[0].setBlocking(false);

    udp_game_server(listener, tcp_sockets, client_ports, playerCount);
}

