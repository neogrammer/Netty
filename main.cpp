// Compile with SFML 3: -lsfml-graphics -lsfml-window -lsfml-network -lsfml-system

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <SFML/System.hpp>
#include <cstdio>
#include <string>
#include <algorithm>   // std::min
#include <cstring>
#include <optional>
#include <unordered_map>
#include <memory>
#include "NetTypes.h"

// ---------- TCP handshake server (returns open TCP sockets) ----------
static bool tcp_handshake_server(sf::TcpSocket out_tcp_sockets[2],
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

// ---------- UDP game server ----------
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

   // for (int i = 0; i < 2; ++i)
      //  tcp_sockets[i].disconnect();
    printf("[Server] Both clients ready. Starting game...\n");

    // ---- Server game state ----
    std::vector<Entity> gameEntities;
    uint32_t nextEntityId = 0;
    uint32_t serverTick = 0;

    // Helper to spawn and broadcast
    auto spawnEntity = [&](float x, float y, uint8_t anim, EntityType etype) -> uint32_t {
        Entity e;
        e.id = nextEntityId++;
        e.x = x; e.y = y;
        e.animation = anim;
        e.animStartTick = serverTick;
        gameEntities.push_back(e);

        // Send SpawnMessage to both players via TCP
        SpawnMessage msg;
        msg.entityId = e.id;
        msg.entityType = etype;
        msg.x = x; msg.y = y;
        msg.animation = anim;

        sf::Packet spawnPacket;
        spawnPacket << static_cast<uint8_t>(NetMsgType::SpawnEntity) << msg;
        sf::Socket::Status status = sf::Socket::Status::Done;
        for (int i = 0; i < 2; ++i)
            status = tcp_sockets[i].send(spawnPacket);

        return e.id;
        };

    // Spawn the two players (replace your old players_x arrays)
    uint32_t p0Id = spawnEntity(100.f, 300.f, 0, EntityType::Player);
    uint32_t p1Id = spawnEntity(700.f, 300.f, 0, EntityType::Player);

    // ---- Main game loop ----
    float players_x[2] = { 0.0f, 0.0f };


    double total_elapsed = 0.0;
    const double TICK_DURATION = 1.0 / 60.0;
    const double MAX_GAME_TIME = 30.0;
    const sf::Time TICK_TIME = sf::seconds(1.f / 60.f);
    sf::Clock gameClock;
    sf::Time accumulator = sf::Time::Zero;
    sf::Time previous_time = gameClock.getElapsedTime();


    //while (total_elapsed < MAX_GAME_TIME) {
    //    sf::Time elapsed = game_clock.restart();
    //    double dt = elapsed.asSeconds();
    //    total_elapsed += dt;
    //    accumulator += sf::seconds((float)dt);

    //    int p0Dir = 0, p1Dir = 0;

    //    // Process incoming input (without printing every packet)
    //    char buf[64];
    //    std::size_t received;
    //    std::optional<sf::IpAddress> sender_ip;
    //    unsigned short sender_port;
    //    while (udp_socket.receive(buf, sizeof(buf) - 1, received,
    //        sender_ip, sender_port) == sf::Socket::Status::Done) {
    //        buf[received] = '\0';

    //        int player_idx = -1;
    //        if (sender_port == client_ports[0]) player_idx = 0;
    //        else if (sender_port == client_ports[1]) player_idx = 1;

    //        if (player_idx >= 0) {
    //            if (std::strchr(buf, 'L')) {
    //                if (player_idx == 0) p0Dir = -1;
    //                else                 p1Dir = -1;
    //            }
    //            if (std::strchr(buf, 'R')) {
    //                if (player_idx == 0) p0Dir = 1;
    //                else                 p1Dir = 1;
    //            }
    //        }
    //    }

    //    // Fixed timestep simulation & broadcast
    //    while (accumulator >= sf::seconds((float)TICK_DURATION)) {
    //        accumulator -= sf::seconds((float)TICK_DURATION);

    //        players_x[0] += (float)(p0Dir * 300.0f * TICK_DURATION);
    //        players_x[1] += (float)(p1Dir * 300.0f * TICK_DURATION);

    //        char msg[128];
    //        snprintf(msg, sizeof(msg), "P0:%.2f P1:%.2f", players_x[0], players_x[1]);

    //        for (int i = 0; i < 2; ++i) {
    //            auto status = udp_socket.send(msg, std::strlen(msg),
    //                client_ips[i].value(), client_ports[i]);
    //            if (status != sf::Socket::Status::Done) {
    //                fprintf(stderr, "[Server] UDP send to player %d failed (status %d)\n",
    //                    i, (int)status);
    //            }
    //        }
    //    }
    int p0Facing = 1;  // 1 = right, 0 = left
    int p1Facing = 1;
    while (total_elapsed < MAX_GAME_TIME) {
        sf::Time dt = gameClock.restart();
        total_elapsed += dt.asSeconds();
        accumulator += dt;

        // ---- Process UDP input (unchanged, still reads 'L'/'R') ----
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

        // ---- Fixed timestep simulation ----
        while (accumulator >= sf::seconds((float)(TICK_DURATION))) {
            accumulator -= sf::seconds((float)(TICK_DURATION));
            serverTick++;

            // Update facing based on movement direction (only when moving)
            if (p0Dir != 0) p0Facing = (p0Dir > 0) ? 1 : 0;
            if (p1Dir != 0) p1Facing = (p1Dir > 0) ? 1 : 0;

            // Update player entities (find them by ID)
            for (auto& e : gameEntities) {
                if (e.id == p0Id) {
                    e.x += (float)(p0Dir * 300.0f * TICK_DURATION);
                    printf("Server: player 0 pos = %.1f\n", e.x);
                    // Optionally change animation based on direction:
                    uint8_t newAnim = (p0Dir == 0) ? 0 : 1;  // 0=idle, 1=walk
                    if (newAnim != e.animation) {
                        e.animation = newAnim;
                        e.animStartTick = serverTick;
                    }
                }
                else if (e.id == p1Id) {
                    e.x += (float)(p1Dir * 300.0f * TICK_DURATION);
                    uint8_t newAnim = (p1Dir == 0) ? 0 : 1;
                    if (newAnim != e.animation) {
                        e.animation = newAnim;
                        e.animStartTick = serverTick;
                    }
                }
                // Other entities (NPCs, projectiles) would be updated here
            }

            // ---- Build and send FrameSnapshot ----
            FrameSnapshot snap;
            snap.frameNumber = serverTick;

            for (const auto& e : gameEntities) {
                EntitySnapshot s;
                s.entityId = e.id;
                s.x_quant = quantise(e.x);
                s.y_quant = quantise(e.y);
                s.animation = e.animation;
                s.animStartTick = e.animStartTick;
                // Set facing bit (bit0 = 1 for right, 0 for left)
                if (e.id == p0Id)
                    s.flags = p0Facing;
                else if (e.id == p1Id)
                    s.flags = p1Facing;
                else
                    s.flags = 0;   // non‑player entities default to right for now
                snap.entities.push_back(s);
                printf("Server snapshot: entity %u, x=%.1f, x_quant=%d\n", e.id, e.x, quantise(e.x));

            }

            sf::Packet snapPacket;
            snapPacket << static_cast<uint8_t>(NetMsgType::FrameSnapshot) << snap;
			sf::Socket::Status status = sf::Socket::Status::Done;
            for (int i = 0; i < 2; ++i)
                status = udp_socket.send(snapPacket, client_ips[i].value(), client_ports[i]);
         }

        // Sleep until it's time for the next tick (saves CPU and gives consistent timing)
        sf::Time next_tick = previous_time + TICK_TIME - sf::microseconds(500);
        sf::Time now = gameClock.getElapsedTime();
        if (next_tick > now)
            sf::sleep(next_tick - now);
     }

    printf("[Server] Finished.\n");
}

// ---------- Server entry ----------
static void server() {
    sf::TcpSocket tcp_sockets[2];
    unsigned short client_udp_ports[2];
    if (!tcp_handshake_server(tcp_sockets, client_udp_ports, 13579)) {
        fprintf(stderr, "Handshake failed.\n");
        return;
    }
    udp_game_server(tcp_sockets, client_udp_ports);
}

// ---------- TCP handshake client ----------
static sf::UdpSocket tcp_handshake_client(const sf::IpAddress& server_ip,
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

struct AnimationSet {
	std::unordered_map<AnimType, sf::Texture*> animMap;
	std::unordered_map<AnimType, std::array<std::vector<sf::IntRect>, 2>> animRects; // 0 = left, 1 = right

    std::unordered_map<AnimType, int> frameCounts;  // number of frames in each animation
    std::unordered_map<AnimType, float> animDurations;  // seconds per frame
    std::unordered_map<AnimType, bool> loops;
	std::unordered_map<AnimType, std::optional<AnimType>> nextAnim;  // optional next animation after this one finishes

};

struct ClientEntity {
    std::unique_ptr<sf::Sprite> sprite;   // non‑default‑constructible, so we use a pointer
    AnimationSet* animSet = nullptr;      // pointer to the animation definition (owned elsewhere)
    AnimType currentAnim = AnimType::Idle;
    uint16_t animStartTick = 0;
    float x = 0.f, y = 0.f;
    // bool facingRight = true;  // optional
    ClientEntity() = default;
	ClientEntity(const ClientEntity&) = delete;  // no copy
	ClientEntity& operator=(const ClientEntity&) = delete;  // no copy assignment
    ClientEntity(ClientEntity&& o) : sprite{ std::move(o.sprite) }, animSet{ o.animSet }, currentAnim{ o.currentAnim }, animStartTick{ o.animStartTick }, x{ o.x }, y{ o.y } 
    {
    }

	ClientEntity& operator=(ClientEntity&& o) {
        sprite = std::move(o.sprite);
        animSet = o.animSet;
        currentAnim = o.currentAnim;
        animStartTick = o.animStartTick;
        x = o.x;
        y = o.y;
        return *this;
	}
};

sf::Texture idleTex{ "assets/textures/player/idle.png" }, walkTex{ "assets/textures/player/walk.png" };
void initPlayerAnimations(AnimationSet& animSet)
{
    // fill playerAnimSet.animMap with textures per animation
    // for now we assume you have a texture cache somewhere
    animSet.animMap[AnimType::Idle] = &idleTex;
    animSet.frameCounts[AnimType::Idle] = 8;
    animSet.animDurations[AnimType::Idle] = .1f;  // seconds per frame
    animSet.loops[AnimType::Idle] = true;
    animSet.nextAnim[AnimType::Idle] = std::nullopt;  // no next animation
    animSet.animRects[AnimType::Idle][0] = {
        sf::IntRect({0, 128}, {64, 128}),
        sf::IntRect({64, 128}, {64, 128}),
        sf::IntRect({128, 128}, {64, 128}),
        sf::IntRect({192, 128}, {64, 128}),
        sf::IntRect({256, 128}, {64, 128}),
        sf::IntRect({320, 128}, {64, 128}),
        sf::IntRect({384, 128}, {64, 128}),
        sf::IntRect({448, 128},{ 64, 128})
    };
    animSet.animRects[AnimType::Idle][1] = {
        sf::IntRect({0, 256}, {64, 128}),
        sf::IntRect({64, 256}, {64, 128}),
        sf::IntRect({128, 256}, {64, 128}),
        sf::IntRect({192, 256}, {64, 128}),
        sf::IntRect({256, 256}, {64, 128}),
        sf::IntRect({320, 256}, {64, 128}),
        sf::IntRect({384, 256}, {64, 128}),
        sf::IntRect({448, 256},{ 64, 128})
    };

    animSet.animMap[AnimType::Walk] = &walkTex;
    animSet.frameCounts[AnimType::Walk] = 10;
    animSet.animDurations[AnimType::Walk] = .08f;
    animSet.loops[AnimType::Walk] = true;
    animSet.nextAnim[AnimType::Walk] = std::nullopt;  // no next animation
    animSet.animRects[AnimType::Walk][0] = {
    sf::IntRect({0, 128}, {64, 128}),
    sf::IntRect({64, 128}, {64, 128}),
    sf::IntRect({128, 128}, {64, 128}),
    sf::IntRect({192, 128}, {64, 128}),
    sf::IntRect({256, 128}, {64, 128}),
    sf::IntRect({320, 128}, {64, 128}),
    sf::IntRect({384, 128}, {64, 128}),
    sf::IntRect({448, 128},{ 64, 128}),
    sf::IntRect({512, 128}, {64, 128}),
    sf::IntRect({576, 128},{ 64, 128})
    };
    animSet.animRects[AnimType::Walk][1] = {
        sf::IntRect({0, 256}, {64, 128}),
        sf::IntRect({64, 256}, {64, 128}),
        sf::IntRect({128, 256}, {64, 128}),
        sf::IntRect({192, 256}, {64, 128}),
        sf::IntRect({256, 256}, {64, 128}),
        sf::IntRect({320, 256}, {64, 128}),
        sf::IntRect({384, 256}, {64, 128}),
        sf::IntRect({448, 256},{ 64, 128}),
        sf::IntRect({512, 256}, {64, 128}),
        sf::IntRect({576, 256},{ 64, 128})
    };

}

std::pair<bool, ClientEntity> createClientEntity(std::unordered_map<EntityType, AnimationSet*>& entityAnimSets, EntityType type, float x, float y) {
    ClientEntity ent;
    ent.x = x;
    ent.y = y;
    ent.currentAnim = AnimType::Idle;    // all entities start in idle
    ent.animStartTick = 0;               // will be set later by snapshot

    bool valid = false;

    // Assign the animation set
    auto animIt = entityAnimSets.find(type);
    if (animIt != entityAnimSets.end())
    {
        ent.animSet = animIt->second;
        valid = true;
    }
    else
    {
        ent.animSet = nullptr;   // fallback
    }

    // Create the sprite using the default texture
    if (valid) {
        sf::Texture& tex = *entityAnimSets[type]->animMap[AnimType::Idle];
        ent.sprite = std::make_unique<sf::Sprite>(tex);
    }
    
    // Optionally set origin, scale, etc.
    // ent.sprite->setOrigin(...);

    return std::pair<bool, ClientEntity>{valid, std::move(ent)};
}


// ---------- Client main ----------
static void client(int server_tcp_port) {
    const sf::IpAddress server_ip(24, 35, 13, 61);  // change to real server IP

    unsigned short my_udp_port;
    int player_id;
    sf::TcpSocket tcp_socket;
    sf::UdpSocket udp_socket = tcp_handshake_client(server_ip, server_tcp_port,
        &my_udp_port, &player_id,
        tcp_socket);
    if (udp_socket.getLocalPort() == 0)
        return;

    udp_socket.setBlocking(false);
    tcp_socket.setBlocking(false);

    const unsigned short server_udp_port = 57913;
    bool tcp_ok_received = false;

    // Reliable OK handshake
    printf("[Client %d] Sending UDP 'OK' and waiting for TCP confirmation...\n", player_id);
    while (!tcp_ok_received) {
        const char* ok_msg = "OK";
        auto res = udp_socket.send(ok_msg, std::strlen(ok_msg), server_ip, server_udp_port);

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
    // tcp_socket.disconnect();
    tcp_socket.setBlocking(false);   // keep TCP open in non‑blocking mode
    // ---- Client entity map ----
    std::unordered_map<uint32_t, ClientEntity> entities;

    // We'll store the player's own entity ID once we know it
    uint32_t myEntityId = 0xFFFFFFFF;

    // ---- Graphics and game state ----
    sf::RenderWindow window(sf::VideoMode({ 800, 600 }), "Game Client");
    window.setFramerateLimit(60);

    // Interpolation state
    struct {
        FrameSnapshot prev;
        FrameSnapshot curr;
        sf::Time      lastSnapTime;    // when we received 'curr'
        bool          hasPrev = false;
    } snapState;
    sf::Clock interpClock;
    const sf::Time tickDuration = sf::seconds(1.f / 60.f);   // server tick rate

    //sf::CircleShape local_shape(20);
    //sf::CircleShape remote_shape(20);
    //local_shape.setFillColor(player_id == 0 ? sf::Color::Red : sf::Color::Blue);
    //remote_shape.setFillColor(player_id == 0 ? sf::Color::Blue : sf::Color::Red);

    //sf::Font font;
    //bool font_loaded = font.openFromFile("bubbly.ttf");
    //sf::Text text(font);
    //text.setCharacterSize(20);
    //text.setFillColor(sf::Color::White);

    //float local_x = 0.0f, remote_x = 0.0f;
    //int other_player = (player_id == 0) ? 1 : 0;
    //float prev_remote_x = 0.0f, next_remote_x = 0.0f;
    //sf::Clock snapshot_clock;
    //const float SNAPSHOT_INTERVAL = 1.0f / 60.0f;

    // add a vector of game objects to represent the objects that needs to be displayed on the screen, and update their positions based on the server state
    // allocate a vector of tiles with the number being the number of tiles that fit a screen plus a few extra tiles to account for scrolling

    // setup initial client entities animation sets
    // Example animation set for a player
    AnimationSet playerAnimSet;
	initPlayerAnimations(playerAnimSet);

    std::unordered_map<EntityType, AnimationSet*> entityAnimSets = {
    { EntityType::Player,    &playerAnimSet },
    //{ EntityType::Goblin,    &goblinAnimSet },
    // ...
    };
    

    // Similarly for goblin, projectile, etc.
    //AnimationSet goblinAnimSet;
    // initGoblinAnimations() ...


    while (window.isOpen()) {
        // Events
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
                        if (msg.entityType == EntityType::Player && myEntityId == 0xFFFFFFFF) {
                            myEntityId = msg.entityId;   // remember our own ID
                        }
                    }
                }
                else if (type == NetMsgType::DestroyEntity) {
                    DestroyMessage msg;
                    tcpPacket >> msg;
                    entities.erase(msg.entityId);
                }
            }
        

        // Send input (no console spam)
        //if (window.hasFocus()) {
        //    std::string input;
        //    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left) ||
        //        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
        //        input += 'L';
        //    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right) ||
        //        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
        //        input += 'R';

        //    if (!input.empty()) {
        //        auto res = udp_socket.send(input.c_str(), input.size(),
        //            server_ip, server_udp_port);

        //        // Client-side prediction
        //        const float step = 300.0f * (1.0f / 60.0f);
        //        if (input.find('L') != std::string::npos) local_x -= step;
        //        if (input.find('R') != std::string::npos) local_x += step;
        //    }
        //}
        std::string input;
        if (window.hasFocus()) {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) input += 'L';
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) input += 'R';
			sf::Socket::Status status = sf::Socket::Status::Done;
            if (!input.empty()) {
                status = udp_socket.send(input.c_str(), input.size(), server_ip, server_udp_port);
                printf("Sent input: %s (status %d)\n", input.c_str(), (int)status);
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



        // Receive server state (without printing)
        //char buf[128];
        //std::size_t received;


        // receive a packet from the server, containing the number of each type of object on screen, and updated state based on tags for each entry in the packet..  packet contains SpriteData, which
        // contains position, velocity, type of Object, and its id
		// so make a packet that contains a vector of SpriteDatas, which is a struct that contains position, velocity, type of Object, and its id. 
		// server updates the game objects, then querys what is on screen, and sends that to the clients, which then update their local game objects based on the server state.
        // client now receives that packet of  SpriteDatas, and
        //while (udp_socket.receive(buf, sizeof(buf) - 1, received,
        //    sender, sender_port) == sf::Socket::Status::Done) {
        //    buf[received] = '\0';

        //    float p0, p1;
        //    if (std::sscanf(buf, "P0:%f P1:%f", &p0, &p1) == 2) {
        //        prev_remote_x = next_remote_x;
        //        next_remote_x = (other_player == 0) ? p0 : p1;
        //        snapshot_clock.restart();
        //        local_x = (player_id == 0) ? p0 : p1;   // reconciliation
        //    }
        //}
        // ---- UDP messages (snapshots) ----
        sf::Packet udpPacket;
        std::optional<sf::IpAddress> sender;
        unsigned short senderPort;
        while (udp_socket.receive(udpPacket, sender, senderPort) == sf::Socket::Status::Done) {
            NetMsgType type;
            udpPacket >> type;
            if (type == NetMsgType::FrameSnapshot) {
                // Shift previous snapshot
                snapState.prev = std::move(snapState.curr);
                udpPacket >> snapState.curr;

                printf("Client received snapshot: frame=%u, entities=%zu\n", snapState.curr.frameNumber, snapState.curr.entities.size());
                for (auto& s : snapState.curr.entities)
                    printf("   entity %u: x_quant=%d, y_quant=%d\n", s.entityId, s.x_quant, s.y_quant);
                snapState.lastSnapTime = interpClock.getElapsedTime();
                snapState.hasPrev = true;
            }
        }

        float renderTick{};
        // ---- Update entities from snapshots ----
        if (snapState.hasPrev) {
            sf::Time now = interpClock.getElapsedTime();
            float t = ((now - snapState.lastSnapTime).asSeconds()) / tickDuration.asSeconds();
            t = std::min(t, 1.0f);

            // Interpolated tick number (for animation)
            uint32_t prevTick = snapState.prev.frameNumber;
            uint32_t currTick = snapState.curr.frameNumber;
            renderTick = prevTick + t * (currTick - prevTick);

            for (const auto& snapEnt : snapState.curr.entities) {
                auto it = entities.find(snapEnt.entityId);
                if (it == entities.end()) continue;

                // ---- Position interpolation ----
                float x = unquantise(snapEnt.x_quant);
                float y = unquantise(snapEnt.y_quant);
                // Check previous snapshot for the same entity
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

                // ---- Animation update ----
                AnimType newAnim = static_cast<AnimType>(snapEnt.animation);
                if (newAnim != it->second.currentAnim) {
                    it->second.currentAnim = newAnim;
                    it->second.animStartTick = snapEnt.animStartTick;
                }

                // ---- Compute which frame to show ----
                int frameIdx = 0;
                if (it->second.animSet) {
                    AnimationSet& animSet = *it->second.animSet;
                    float elapsedTicks = std::max<float>(0.f, renderTick - it->second.animStartTick);
                    float timeInSeconds = elapsedTicks * tickDuration.asSeconds();
                    float durationPerFrame = animSet.animDurations[it->second.currentAnim];
                    int totalFrames = animSet.frameCounts[it->second.currentAnim];
                    if (totalFrames > 0) {
                        int rawFrame = static_cast<int>(timeInSeconds / durationPerFrame);
                        if (animSet.loops[it->second.currentAnim]) {
                            frameIdx = rawFrame % totalFrames;
                        }
                        else {
                            frameIdx = std::min(rawFrame, totalFrames - 1);
                        }
                    }

                    // Choose left or right texture rects (facing)
                    // For now, assume right (index 1) unless we add a facing flag.
                    // You can later use snapEnt.flags & 1 to decide.


                    // Also update the sprite's texture if the animation changed (already done via setTextureRect, but you may need to set the texture sheet)
                    // Since all frames are on the same texture sheet (idleTex or walkTex), you only need to set the texture once per entity type.
                    // In createClientEntity you already set the texture to idleTex; you could switch texture when animation changes if they use different sheets.
                    // Here we assume all animations share a common sheet, or we could swap texture:
                    if (animSet.animMap.count(it->second.currentAnim)) {
                        sf::Texture* tex = animSet.animMap[it->second.currentAnim];
                        if (tex) it->second.sprite->setTexture(*tex);  // false = keep texture rect
                        //int facing = snapEnt.flags & 1; // 0 = left, 1 = right
                        uint8_t facing = 0; // default
                        auto snapIt = std::find_if(snapState.curr.entities.begin(), snapState.curr.entities.end(),
                            [&](const EntitySnapshot& s) { return s.entityId == it->first; });
                        if (snapIt != snapState.curr.entities.end())
                            facing = snapIt->flags & 1;
                        if (animSet.animRects.count(it->second.currentAnim) &&
                            animSet.animRects[it->second.currentAnim][facing].size() > static_cast<size_t>(frameIdx)) {
                            sf::IntRect rect = animSet.animRects[it->second.currentAnim][facing][frameIdx];
                            it->second.sprite->setTextureRect(rect);
                        }
                    }
                }
                it->second.x = x;
                printf("Client: entity %u pos = %.1f\n", snapEnt.entityId, x);
            }

        }


        window.clear();
        for (auto& [id, ent] : entities) {
            int frameIdx = 0;
            if (ent.animSet) {
                AnimationSet& anim = *ent.animSet;
                AnimType cur = ent.currentAnim;
                float elapsedTicks = renderTick - ent.animStartTick;
                float timeSec = elapsedTicks * tickDuration.asSeconds();   // tickDuration = 1/60.f

                float durationPerFrame = anim.animDurations[cur];
                int totalFrames = anim.frameCounts[cur];

                if (totalFrames > 0) {
                    int rawFrame = static_cast<int>(timeSec / durationPerFrame);
                    frameIdx = anim.loops[cur] ? (rawFrame % totalFrames)
                        : std::min(rawFrame, totalFrames - 1);
                }

                // facing: 0 = left, 1 = right
                //uint8_t facing = snapState.curr.entities[id].flags & 1;

                uint8_t facing = 0; // default
                auto snapIt = std::find_if(snapState.curr.entities.begin(), snapState.curr.entities.end(),
                    [&](const EntitySnapshot& s) { return s.entityId == id; });
                if (snapIt != snapState.curr.entities.end())
                    facing = snapIt->flags & 1;

                // Also ensure the correct texture is bound (if different sheets for each animation)
                if (anim.animMap.count(cur)) {
                    sf::Texture* tex = anim.animMap[cur];
                    if (tex) ent.sprite->setTexture(*tex);
                    if (anim.animRects.count(cur) &&
                        anim.animRects[cur][facing].size() > frameIdx) {
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

// ---------- Main ----------
int main(int argc, char* argv[]) {
    if (argc > 1 && std::strcmp(argv[1], "client") == 0)
        client(13579);
    else
        server();
    return 0;
}