// Compile with SFML 3: -lsfml-graphics -lsfml-window -lsfml-network -lsfml-system

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <SFML/System.hpp>
#include <cstdio>
#include <string>
#include <algorithm>   // std::min
#include <cstring>
#include <optional>

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

    for (int i = 0; i < 2; ++i)
        tcp_sockets[i].disconnect();
    printf("[Server] Both clients ready. Starting game...\n");

    // ---- Main game loop ----
    float players_x[2] = { 0.0f, 0.0f };


    double total_elapsed = 0.0;
    const double TICK_DURATION = 1.0 / 60.0;
    const double MAX_GAME_TIME = 30.0;
    const sf::Time TICK_TIME = sf::seconds(1.0 / 60.0);
    sf::Clock game_clock;
    sf::Time accumulator = sf::Time::Zero;
    sf::Time previous_time = game_clock.getElapsedTime();


    while (total_elapsed < MAX_GAME_TIME) {
        sf::Time elapsed = game_clock.restart();
        double dt = elapsed.asSeconds();
        total_elapsed += dt;
        accumulator += sf::seconds(dt);

        int p0Dir = 0, p1Dir = 0;

        // Process incoming input (without printing every packet)
        char buf[64];
        std::size_t received;
        std::optional<sf::IpAddress> sender_ip;
        unsigned short sender_port;
        while (udp_socket.receive(buf, sizeof(buf) - 1, received,
            sender_ip, sender_port) == sf::Socket::Status::Done) {
            buf[received] = '\0';

            int player_idx = -1;
            if (sender_port == client_ports[0]) player_idx = 0;
            else if (sender_port == client_ports[1]) player_idx = 1;

            if (player_idx >= 0) {
                if (std::strchr(buf, 'L')) {
                    if (player_idx == 0) p0Dir = -1;
                    else                 p1Dir = -1;
                }
                if (std::strchr(buf, 'R')) {
                    if (player_idx == 0) p0Dir = 1;
                    else                 p1Dir = 1;
                }
            }
        }

        // Fixed timestep simulation & broadcast
        while (accumulator >= sf::seconds(TICK_DURATION)) {
            accumulator -= sf::seconds(TICK_DURATION);

            players_x[0] += (float)(p0Dir * 300.0f * TICK_DURATION);
            players_x[1] += (float)(p1Dir * 300.0f * TICK_DURATION);

            char msg[128];
            snprintf(msg, sizeof(msg), "P0:%.2f P1:%.2f", players_x[0], players_x[1]);

            for (int i = 0; i < 2; ++i) {
                auto status = udp_socket.send(msg, std::strlen(msg),
                    client_ips[i].value(), client_ports[i]);
                if (status != sf::Socket::Status::Done) {
                    fprintf(stderr, "[Server] UDP send to player %d failed (status %d)\n",
                        i, (int)status);
                }
            }
        }

        // Sleep until it's time for the next tick (saves CPU and gives consistent timing)
        sf::Time next_tick = previous_time + TICK_TIME - sf::microseconds(500);
        sf::Time now = game_clock.getElapsedTime();
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
    tcp_socket.disconnect();

    // ---- Graphics and game state ----
    sf::RenderWindow window(sf::VideoMode({ 800, 600 }), "Game Client");
    window.setFramerateLimit(60);

    sf::CircleShape local_shape(20);
    sf::CircleShape remote_shape(20);
    local_shape.setFillColor(player_id == 0 ? sf::Color::Red : sf::Color::Blue);
    remote_shape.setFillColor(player_id == 0 ? sf::Color::Blue : sf::Color::Red);

    sf::Font font;
    bool font_loaded = font.openFromFile("bubbly.ttf");
    sf::Text text(font);
    text.setCharacterSize(20);
    text.setFillColor(sf::Color::White);

    float local_x = 0.0f, remote_x = 0.0f;
    int other_player = (player_id == 0) ? 1 : 0;
    float prev_remote_x = 0.0f, next_remote_x = 0.0f;
    sf::Clock snapshot_clock;
    const float SNAPSHOT_INTERVAL = 1.0f / 60.0f;

    while (window.isOpen()) {
        // Events
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        // Send input (no console spam)
        if (window.hasFocus()) {
            std::string input;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
                input += 'L';
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
                input += 'R';

            if (!input.empty()) {
                udp_socket.send(input.c_str(), input.size(),
                    server_ip, server_udp_port);

                // Client-side prediction
                const float step = 300.0f * (1.0f / 60.0f);
                if (input.find('L') != std::string::npos) local_x -= step;
                if (input.find('R') != std::string::npos) local_x += step;
            }
        }

        // Receive server state (without printing)
        char buf[128];
        std::size_t received;
        std::optional<sf::IpAddress> sender;
        unsigned short sender_port;
        while (udp_socket.receive(buf, sizeof(buf) - 1, received,
            sender, sender_port) == sf::Socket::Status::Done) {
            buf[received] = '\0';

            float p0, p1;
            if (std::sscanf(buf, "P0:%f P1:%f", &p0, &p1) == 2) {
                prev_remote_x = next_remote_x;
                next_remote_x = (other_player == 0) ? p0 : p1;
                snapshot_clock.restart();
                local_x = (player_id == 0) ? p0 : p1;   // reconciliation
            }
        }

        // Interpolate remote player
        float elapsed = snapshot_clock.getElapsedTime().asSeconds();
        float t = std::min<float>(elapsed / SNAPSHOT_INTERVAL, 1.0f);
        remote_x = prev_remote_x + t * (next_remote_x - prev_remote_x);

        // Render
        window.clear();
        local_shape.setPosition({ local_x, 300.f });
        remote_shape.setPosition({ remote_x, 350.f });
        window.draw(local_shape);
        window.draw(remote_shape);
        if (font_loaded) {
            char info[128];
            snprintf(info, sizeof(info), "Player %d  me: %.2f  other: %.2f",
                player_id, local_x, remote_x);
            text.setString(info);
            window.draw(text);
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