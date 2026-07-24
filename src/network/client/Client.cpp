#include "Client.h"
#include <network/NetworkCommon.h>
#include <NetTypes.h>
#include <entities/Entity.h>
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <SFML/System.hpp>
#include <unordered_map>
#include <cstring>
#include <cstdio>
#include <algorithm>

void run_client(int server_tcp_port) {
    const sf::IpAddress server_ip(24, 35, 13, 61);  // change to real IP

    unsigned short my_udp_port;
    int player_id;
    sf::TcpSocket tcp_socket;
    sf::UdpSocket udp_socket = tcp_handshake_client(server_ip, server_tcp_port,
        &my_udp_port, &player_id, tcp_socket);
    if (udp_socket.getLocalPort() == 0)
        return;

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

    // ---- Animation setup ----
    sf::Texture idleTex("assets/textures/player/idle.png");
    sf::Texture walkTex("assets/textures/player/walk.png");
    AnimationSet playerAnimSet;
    initPlayerAnimations(playerAnimSet, idleTex, walkTex);
    std::unordered_map<EntityType, AnimationSet*> entityAnimSets = {
        { EntityType::Player, &playerAnimSet }
    };

    // ---- Entity map and snapshot interpolation ----
    std::unordered_map<uint32_t, ClientEntity> entities;
    uint32_t myEntityId = 0xFFFFFFFF;

    struct {
        FrameSnapshot prev;
        FrameSnapshot curr;
        sf::Time      lastSnapTime;
        bool          hasPrev = false;
    } snapState;
    sf::Clock interpClock;
    const sf::Time tickDuration = sf::seconds(1.f / 60.f);

    sf::RenderWindow window(sf::VideoMode({ 800, 600 }), "Game Client");
    window.setFramerateLimit(60);

    while (window.isOpen()) {
        // ---- Events ----
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        // ---- TCP messages (spawn / destroy) ----
        sf::Packet tcpPacket;
        while (tcp_socket.receive(tcpPacket) == sf::Socket::Status::Done) {
            NetMsgType type;
            tcpPacket >> type;
            if (type == NetMsgType::SpawnEntity) {
                SpawnMessage msg;
                tcpPacket >> msg;
                auto [ok, ent] = createClientEntity(entityAnimSets, msg.entityType, msg.x, msg.y);
                if (ok) {
                    ent.currentAnim = static_cast<AnimType>(msg.animation);
                    entities[msg.entityId] = std::move(ent);
                    if (msg.entityType == EntityType::Player && myEntityId == 0xFFFFFFFF)
                        myEntityId = msg.entityId;
                }
            }
            else if (type == NetMsgType::DestroyEntity) {
                DestroyMessage msg;
                tcpPacket >> msg;
                entities.erase(msg.entityId);
            }
        }

        // ---- Send input ----
        std::string input;
        if (window.hasFocus()) {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) input += 'L';
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) input += 'R';
            if (!input.empty()) {
                udp_socket.send(input.c_str(), input.size(), server_ip, server_udp_port);
                // Client-side prediction
                if (myEntityId != 0xFFFFFFFF) {
                    auto it = entities.find(myEntityId);
                    if (it != entities.end()) {
                        float step = 300.0f * (1.f / 60.f);
                        if (input.find('L') != std::string::npos) it->second.x -= step;
                        if (input.find('R') != std::string::npos) it->second.x += step;
                    }
                }
            }
        }

        // ---- UDP messages (snapshots) ----
        sf::Packet udpPacket;
        std::optional<sf::IpAddress> sender;
        unsigned short senderPort;
        while (udp_socket.receive(udpPacket, sender, senderPort) == sf::Socket::Status::Done) {
            NetMsgType type;
            udpPacket >> type;
            if (type == NetMsgType::FrameSnapshot) {
                snapState.prev = std::move(snapState.curr);
                udpPacket >> snapState.curr;
                snapState.lastSnapTime = interpClock.getElapsedTime();
                snapState.hasPrev = true;
            }
        }

        // ---- Update entities from snapshots ----
        float renderTick = 0.f;
        if (snapState.hasPrev) {
            sf::Time now = interpClock.getElapsedTime();
            float t = ((now - snapState.lastSnapTime).asSeconds()) / tickDuration.asSeconds();
            t = std::min(t, 1.0f);

            uint32_t prevTick = snapState.prev.frameNumber;
            uint32_t currTick = snapState.curr.frameNumber;
            renderTick = prevTick + t * (currTick - prevTick);

            for (const auto& snapEnt : snapState.curr.entities) {
                auto it = entities.find(snapEnt.entityId);
                if (it == entities.end()) continue;

                // Interpolate position
                float x = unquantise(snapEnt.x_quant);
                float y = unquantise(snapEnt.y_quant);
                auto prevIt = std::find_if(snapState.prev.entities.begin(), snapState.prev.entities.end(),
                    [&](const EntitySnapshot& s) { return s.entityId == snapEnt.entityId; });
                if (prevIt != snapState.prev.entities.end()) {
                    float prevX = unquantise(prevIt->x_quant);
                    float prevY = unquantise(prevIt->y_quant);
                    x = prevX + t * (x - prevX);
                    y = prevY + t * (y - prevY);
                }
                it->second.x = x;
                it->second.y = y;

                // Animation state
                AnimType newAnim = static_cast<AnimType>(snapEnt.animation);
                if (newAnim != it->second.currentAnim) {
                    it->second.currentAnim = newAnim;
                    it->second.animStartTick = snapEnt.animStartTick;
                }
            }
        }

        // ---- Render ----
        window.clear();
        for (auto& [id, ent] : entities) {
            int frameIdx = 0;
            if (ent.animSet) {
                AnimationSet& anim = *ent.animSet;
                AnimType cur = ent.currentAnim;
                float elapsedTicks = renderTick - ent.animStartTick;
                float timeSec = elapsedTicks * tickDuration.asSeconds();

                float durPerFrame = anim.animDurations[cur];
                int totalFrames = anim.frameCounts[cur];
                if (totalFrames > 0) {
                    int rawFrame = static_cast<int>(timeSec / durPerFrame);
                    frameIdx = anim.loops[cur] ? (rawFrame % totalFrames)
                        : std::min(rawFrame, totalFrames - 1);
                }

                // Determine facing (flags bit0)
                uint8_t facing = 0;
                auto snapIt = std::find_if(snapState.curr.entities.begin(), snapState.curr.entities.end(),
                    [&](const EntitySnapshot& s) { return s.entityId == id; });
                if (snapIt != snapState.curr.entities.end())
                    facing = snapIt->flags & 1;

                // Apply texture and rect
                if (anim.animMap.count(cur)) {
                    sf::Texture* tex = anim.animMap[cur];
                    if (tex) ent.sprite->setTexture(*tex);
                    if (anim.animRects.count(cur) &&
                        anim.animRects[cur][facing].size() > static_cast<size_t>(frameIdx)) {
                        sf::IntRect rect = anim.animRects[cur][facing][frameIdx];
                        ent.sprite->setTextureRect(rect);
                    }
                }
            }
            if (ent.sprite) {
                ent.sprite->setPosition({ ent.x, ent.y });
                window.draw(*ent.sprite);
            }
        }
        window.display();
    }
}