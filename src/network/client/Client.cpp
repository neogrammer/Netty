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
#include <network/client/ClientContext.h>
#include <mgmt/GameStateManager.h>
#include <entities/animation/AnimationSet.h>
#include <res/SceneResources.h>
#include <res/Cfg.h>

void run_client(int server_tcp_port) {
    const sf::IpAddress server_ip(24, 35, 13, 61);  // change to real IP

    // ---------- TCP handshake ----------
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

    printf("[Client %d] Starting main loop, Animation setup correctly...\n", player_id);

    // ---------- Client context ----------
    ClientContext ctx{};
    ctx.udpSocket = &udp_socket;
    ctx.tcpSocket = &tcp_socket;
    ctx.serverIp = server_ip;
    ctx.serverPort = server_udp_port;
    ctx.playerId = player_id;
    ctx.myEntityId = player_id;
    ctx.currentZone = 1;
    ctx.currentLevel = 1;

    // ---------- Window ----------
    sf::RenderWindow window(sf::VideoMode({ 1600, 900 }), "Game Client");
    window.setFramerateLimit(60);

    GameStateManager gsm(ctx);
    ctx.gsm = &gsm;

    gsm.registerState("title", std::make_unique<TitleState>(&window, ctx));
    gsm.registerState("overworld", std::make_unique<OverworldState>(&window, ctx, entityAnimSets));

    gsm.registerState("play", std::make_unique<PlayState>(&window, ctx, entityAnimSets));
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

        // ---- TCP processing (centralised) ----
        sf::Packet tcpPacket;
        while (tcp_socket.receive(tcpPacket) == sf::Socket::Status::Done) {
            NetMsgType type;
            tcpPacket >> type;
            if (type == NetMsgType::AssignPlayerEntity) {
                AssignPlayerMessage msg;
                tcpPacket >> msg;
                ctx.myEntityId = msg.entityId;
                printf("[Client %d] Assigned player entity %u\n", player_id, msg.entityId);
            }
            else if (type == NetMsgType::LoadZone) {
                LoadZoneMessage msg;
                tcpPacket >> msg;
                ctx.currentZone = msg.zoneNumber;
				ctx.currentLevel = -1;  // reset level when loading a new zone
                // Load zone assets immediately (will be used by OverworldState)
                // We need a SceneResources to load here. We'll store it in a global or in context.
                // For simplicity, we'll let OverworldState load on enter; but to avoid delay,
                // we can pre-load here. We'll add a SceneResources* to context temporarily.
                printf("[Client %d] Loading zone %d\n", player_id, msg.zoneNumber);
                // We'll call loadForScene later inside OverworldState::enter(), because we need the instance.
                // Just store the zone number; OverworldState will load assets when entered.
                gsm.switchTo("overworld");
            }
            else if (type == NetMsgType::LoadLevel) {
                LoadLevelMessage msg;
                tcpPacket >> msg;
                ctx.currentLevel = msg.levelNumber;
				ctx.currentZone = -1;  // reset zone when loading a level
                printf("[Client %d] Loading level %d\n", player_id, msg.levelNumber);
                // PlayState will load assets on enter
                gsm.switchTo("play");
            }
            else if (type == NetMsgType::SpawnEntity) {
                SpawnMessage msg;
                tcpPacket >> msg;
                // We need to store the spawned entity in the current state's entities map.
                // Since processing is central, we delegate to the active state via a virtual method.
                // Let's add a `handleSpawn(msg)` to IGameState.
                // But to keep it simple, we'll just buffer the spawns in a queue in ClientContext,
                // and each state processes them in its update.
                // Quick solution: add a vector<SpawnMessage> to ClientContext.
                // But we'll implement a cleaner method: make IGameState have `processSpawn(msg)`.
                // For now, we'll add a `pendingSpawns` queue in context.
                ctx.pendingSpawns.push_back(msg);   // requires #include <vector>
            }
            else if (type == NetMsgType::DestroyEntity) {
                DestroyMessage msg;
                tcpPacket >> msg;
                ctx.pendingDestroys.push_back(msg);
            }
        }

        // ---- UDP processing (centralised) ----
        // We'll process snapshots centrally and push them to the current state.
        sf::Packet udpPacket;
        std::optional<sf::IpAddress> sender;
        unsigned short senderPort;
        while (udp_socket.receive(udpPacket, sender, senderPort) == sf::Socket::Status::Done) {
            NetMsgType type;
            udpPacket >> type;
            if (type == NetMsgType::FrameSnapshot) {
                ctx.latestSnapshot = std::move(udpPacket);   // store raw packet for state to deserialize later
                ctx.hasSnapshot = true;
            }
        }

        // ---- Input sending (centralised) ----
        // We'll read keyboard and send movement input here, regardless of state.
        if (window.hasFocus()) {
            std::string input;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) input += 'L';
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) input += 'R';
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter)) input += 'E'; // execute action
            if (!input.empty()) {
                udp_socket.send(input.c_str(), input.size(), server_ip, server_udp_port);
                // Client-side prediction can be done in states using context.myEntityId
                // Client‑side prediction: move own player immediately
                if (ctx.myEntityId != 0xFFFFFFFF) {
                    // We need access to the current state's entity map.
                    // Since we're in the central loop, we can use the GameStateManager to get the current state.
                    // A simpler way: move the entity directly in the state's update using the input.
                    // We'll store the input string in context and let the state apply prediction.
                    ctx.latestInput = input;   // new field in ClientContext
                }
            }
        }

        // Update & draw current state
        gsm.update(clock.restart());
        gsm.draw(window);
    }

}