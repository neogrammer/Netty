#include "PlayState.h"
#include <cstdio>
#include <algorithm>
#include <cstring>

PlayState::PlayState(sf::RenderWindow* win,
    sf::UdpSocket* udp,
    sf::TcpSocket* tcp,
    const sf::IpAddress& srvIp,
    unsigned short srvPort,
    int pid,
    const std::unordered_map<EntityType, AnimationSet*>& animSets)
    : window(win), udpSocket(udp), tcpSocket(tcp), serverIp(srvIp),
    serverPort(srvPort), playerId(pid), entityAnimSets(animSets)
{
    printf("sizeof(EntitySnapshot) = %zu\n", sizeof(EntitySnapshot));
}

void PlayState::enter() {
    printf("sizeof(EntitySnapshot) = %zu\n", sizeof(EntitySnapshot));
    printf("[PlayState] Entered for player %d\n", playerId);
    // Immediately create any entities that were spawned while we were in another state
    processTCPMessages();
    // Reset interpolation state – we’ll start fresh from the next snapshot
    snapState.hasPrev = false;
    interpClock.restart();
}

void PlayState::exit() {
    printf("[PlayState] Exiting\n");
    entities.clear();
}

void PlayState::handleEvent(const sf::Event& event) {
    // Not used for gameplay input
}

void PlayState::update(sf::Time dt) {
    processTCPMessages();
    sendInput();
    processSnapshots();

    // Calculate interpolation tick
    currentRenderTick = 0.f;
    if (snapState.hasPrev) {
        sf::Time now = interpClock.getElapsedTime();
        float t = ((now - snapState.lastSnapTime).asSeconds()) / tickDuration.asSeconds();
        t = std::min(t, 1.0f);
        uint32_t prevTick = snapState.prev.frameNumber;
        uint32_t currTick = snapState.curr.frameNumber;
        currentRenderTick = prevTick + t * (currTick - prevTick);
        interpolateEntities(currentRenderTick);
        // if no snapshot came in, we still interpolated, so we can do collision detection here on the client
        // collide check all entities here and store em in a list of collisions, then sort by time of collision, then resolve them in order
        // then update positions and velocities accordingly, dont worry about the animation, just keep running the one it interpolated. 
        // else then 
        // if the snapshots came in, then we just interpolated to the latest snapshot, and trust the server to have done collision detection and resolution, so we dont need to do it here.
        // end if

        // everything is in its final spot for this frame, now for the player entity, interpolate the center of the client view to snap to keep the player in the center of the screen if it can scroll,
        // and while int the center of the screen, slowly interpolate to the player center to keep the player centered but snap hard when they go out of boundarys if the screen can scroll, 
        // or snap view to bounds of level and background

        // good to draw the frame now

        printf("[Client %d] Interpolated entities at render tick %.2f\n", playerId, currentRenderTick);
    }
}

void PlayState::draw(sf::RenderWindow& window) {
    printf("[Client %d] Drawing %zu entities\n", playerId, entities.size());
    window.clear();

	// draw parallax background here

    // draw part of map that is on same level as player, the ground

    // draw the game entities
    for (auto& [id, ent] : entities) {
        if (!ent.sprite) continue;

        int frameIdx = 0;
        if (ent.animSet) {
            AnimationSet& anim = *ent.animSet;
            AnimType cur = ent.currentAnim;

            // Calculate animation frame
            float elapsedTicks = currentRenderTick - ent.animStartTick;
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
            // Look up the entity in the current snapshot (if available) to get flags
            if (snapState.hasPrev) {
                auto snapIt = std::find_if(snapState.curr.entities.begin(),
                    snapState.curr.entities.end(),
                    [&](const EntitySnapshot& s) { return s.entityId == id; });
                if (snapIt != snapState.curr.entities.end())
                    facing = snapIt->flags & 1;
            }

            // Apply the correct texture and texture rect
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

        ent.sprite->setPosition({ ent.x, ent.y });
        window.draw(*ent.sprite);
    }

	// draw any info overlays like damage

	// draw foreground elements like trees, walls, etc.

	// draw UI elements like health bars, score, etc.

    window.display();
}

// ---------- Private helpers (identical to old code) ----------

void PlayState::processTCPMessages() {
    sf::Packet packet;
    while (tcpSocket->receive(packet) == sf::Socket::Status::Done) {
        NetMsgType type;
        packet >> type;
        if (type == NetMsgType::SpawnEntity) {
            SpawnMessage msg;
            packet >> msg;
            printf("[Client %d] Received spawn: entity %u type %d pos (%.1f, %.1f)\n",
                playerId, msg.entityId, (int)msg.entityType, msg.x, msg.y);
            auto [ok, ent] = createClientEntity(entityAnimSets, msg.entityType, msg.x, msg.y);
            if (ok) {
                ent.currentAnim = static_cast<AnimType>(msg.animation);
                entities[msg.entityId] = std::move(ent);
                // DO NOT set myEntityId here anymore
            }
        }
        else if (type == NetMsgType::DestroyEntity) {
            DestroyMessage msg;
            packet >> msg;
            entities.erase(msg.entityId);
            if (myEntityId == msg.entityId)
                myEntityId = 0xFFFFFFFF;   // reset if our player was destroyed
        }
        else if (type == NetMsgType::AssignPlayerEntity) {
            AssignPlayerMessage msg;
            packet >> msg;
            myEntityId = msg.entityId;
            printf("[Client %d] Assigned player entity %u\n", playerId, myEntityId);
        }
    }
}
void PlayState::sendInput() {
    if (!window->hasFocus()) return;
    std::string input;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) input += 'L';
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) input += 'R';
    if (input.empty()) return;

    udpSocket->send(input.c_str(), input.size(), serverIp, serverPort);

    // Client-side prediction
    if (myEntityId != 0xFFFFFFFF) {
        auto it = entities.find(myEntityId);
        if (it != entities.end()) {
            float step = 300.0f * (1.f / 60.f);
            if (input.find('L') != std::string::npos) it->second.x -= step;
            if (input.find('R') != std::string::npos) it->second.x += step;
        }
    }
    else
    {
		printf("[Client %d] Warning: myEntityId is not assigned yet!\n", playerId);
    }
}

void PlayState::processSnapshots() {
    sf::Packet packet;
    std::optional<sf::IpAddress> sender;
    unsigned short senderPort;
    while (udpSocket->receive(packet, sender, senderPort) == sf::Socket::Status::Done) {
        NetMsgType type;
        packet >> type;
        if (type == NetMsgType::FrameSnapshot) {
            // dump raw bytes of the whole UDP packet
            const void* raw = packet.getData();
            std::size_t sz = packet.getDataSize();
            printf("[Client %d] Raw snapshot (%zu bytes): ", playerId, sz);
            for (std::size_t i = 0; i < sz && i < 80; ++i)
                printf("%02x ", ((const unsigned char*)raw)[i]);
            printf("\n");

            printf("[Client %d] Got snapshot frame %u with %zu entities\n",
                playerId, snapState.curr.frameNumber, snapState.curr.entities.size());
            snapState.prev = std::move(snapState.curr);
            packet >> snapState.curr;
            snapState.lastSnapTime = interpClock.getElapsedTime();
            snapState.hasPrev = true;

            // -------- diagnostic --------
            printf("[Client %d] Snapshot frame %u IDs: ", playerId, snapState.curr.frameNumber);
            for (const auto& e : snapState.curr.entities)
                printf("%u ", e.entityId);
            printf("\n");
            // --------------------------

            if (!snapState.curr.entities.empty()) {
                const auto& e0 = snapState.curr.entities[0];
                printf("[Client %d] First entity raw: id=%u x=%d y=%d anim=%u tick=%u flags=%u\n",
                    playerId, e0.entityId, e0.x_quant, e0.y_quant,
                    e0.animation, e0.animStartTick, e0.flags);
            }
        }
    }
}
void PlayState::interpolateEntities(float renderTick) {
    if (!snapState.hasPrev) return;

    float t = 0.f;
    if (snapState.curr.frameNumber != snapState.prev.frameNumber) {
        t = (renderTick - snapState.prev.frameNumber) /
            (float)(snapState.curr.frameNumber - snapState.prev.frameNumber);
    }
    t = std::min(t, 1.0f);

    for (const auto& snapEnt : snapState.curr.entities) {
        auto it = entities.find(snapEnt.entityId);
        if (it == entities.end()) continue;

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
        if (snapEnt.entityId == 0) {   // the remote player on Client 1
            printf("[Client %d] Entity 0: raw=(%d,%d) unq=(%.1f,%.1f) prev raw? %s -> final x=%.1f (t=%.2f)\n",
                playerId,
                snapEnt.x_quant, snapEnt.y_quant,
                unquantise(snapEnt.x_quant), unquantise(snapEnt.y_quant),
                prevIt != snapState.prev.entities.end() ? "yes" : "no",
                x, t);
        }
        if (snapEnt.entityId == 1) {   // remote player for client 0
            printf("[Client %d] Entity 1: raw=(%d,%d) unq=(%.1f,%.1f) prev raw? %s -> final x=%.1f (t=%.2f)\n",
                playerId,
                snapEnt.x_quant, snapEnt.y_quant,
                unquantise(snapEnt.x_quant), unquantise(snapEnt.y_quant),
                prevIt != snapState.prev.entities.end() ? "yes" : "no",
                x, t);
        }

        AnimType newAnim = static_cast<AnimType>(snapEnt.animation);
        if (newAnim != it->second.currentAnim) {
            it->second.currentAnim = newAnim;
            it->second.animStartTick = snapEnt.animStartTick;
        }
    }
}