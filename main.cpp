// Compile with SFML 3 linked (graphics, window, system)
// On Windows: add ws2_32.lib; SFML auto‑links if using #pragma comment.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// ---------- platform adaptation ----------
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define CLOSE_SOCKET(s)   closesocket(s)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#define SOCKET          int
#define CLOSE_SOCKET(s) close(s)
#endif

// ---------- error printing ----------
static void print_error(const char* msg) {
#ifdef _WIN32
    fprintf(stderr, "%s: %d\n", msg, WSAGetLastError());
#else
    perror(msg);
#endif
}

// ---------- set socket non‑blocking ----------
static int set_nonblocking(SOCKET sock) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

// ---------- simple sleep (cross‑platform) ----------
static void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

// ---------- TCP handshake server (unchanged) ----------
static int tcp_handshake_server(unsigned short* out_client_ports) {
    SOCKET listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { print_error("TCP socket"); return -1; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;   // OS picks a free TCP port

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        print_error("TCP bind"); CLOSE_SOCKET(listen_fd); return -1;
    }

    int addr_len = sizeof(addr);
    getsockname(listen_fd, (struct sockaddr*)&addr, &addr_len);
    unsigned short tcp_port = ntohs(addr.sin_port);
    printf("TCP handshake on port %d\n", tcp_port);

    if (listen(listen_fd, 2) != 0) {
        print_error("TCP listen"); CLOSE_SOCKET(listen_fd); return -1;
    }

    for (int i = 0; i < 2; i++) {
        printf("Waiting for player %d...\n", i + 1);
        struct sockaddr_storage caddr;
        int caddr_len = sizeof(caddr);
        SOCKET cfd = accept(listen_fd, (struct sockaddr*)&caddr, &caddr_len);
        if (cfd < 0) { print_error("accept"); CLOSE_SOCKET(listen_fd); return -1; }

        unsigned short server_udp_port = 12345;  // fixed UDP port
        int player_id = i;
        send(cfd, (const char*)&player_id, sizeof(player_id), 0);
        send(cfd, (const char*)&server_udp_port, sizeof(server_udp_port), 0);

        unsigned short client_udp_port = 0;
        if (recv(cfd, (char*)&client_udp_port, sizeof(client_udp_port), 0) != sizeof(client_udp_port)) {
            print_error("TCP recv client port"); CLOSE_SOCKET(cfd); CLOSE_SOCKET(listen_fd); return -1;
        }
        out_client_ports[i] = client_udp_port;
        printf("Player %d registered (UDP port %d)\n", i, client_udp_port);

        CLOSE_SOCKET(cfd);
    }
    CLOSE_SOCKET(listen_fd);
    return 0;
}

// ---------- UDP game server (IMPROVED) ----------
static void udp_game_server(unsigned short client_ports[2]) {
    SOCKET udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0) { print_error("UDP socket"); return; }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(12345);

    if (bind(udp_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        print_error("UDP bind"); CLOSE_SOCKET(udp_fd); return;
    }
    set_nonblocking(udp_fd);

    struct sockaddr_in client_addrs[2];
    for (int i = 0; i < 2; i++) {
        memset(&client_addrs[i], 0, sizeof(client_addrs[i]));
        client_addrs[i].sin_family = AF_INET;
        client_addrs[i].sin_addr.s_addr = inet_addr("127.0.0.1");
        client_addrs[i].sin_port = htons(client_ports[i]);
    }

    float players_x[2] = { 0.0f, 0.0f };

    const double TICK_DURATION = 1.0 / 60.0;   // 60 Hz simulation
    clock_t previous_clock = clock();
    double accumulator = 0.0;
    double total_elapsed = 0.0;
    const double MAX_GAME_TIME = 30.0;
    int running = 1;

    printf("UDP game server started. Running for %.0f seconds...\n", MAX_GAME_TIME);

    while (running && total_elapsed < MAX_GAME_TIME) {
        // 1. Compute elapsed real time
        clock_t current_clock = clock();
        double dt = (double)(current_clock - previous_clock) / CLOCKS_PER_SEC;
        previous_clock = current_clock;
        total_elapsed += dt;
        accumulator += dt;

        // 2. Drain ALL pending input from the UDP socket (non‑blocking)
        {
            char buf[64];
            struct sockaddr_in from;
            socklen_t fromlen = sizeof(from);
            int n;
            while ((n = recvfrom(udp_fd, buf, sizeof(buf) - 1, 0,
                (struct sockaddr*)&from, &fromlen)) > 0) {
                buf[n] = '\0';

                // Identify the sender by comparing port
                int player_idx = -1;
                for (int i = 0; i < 2; i++) {
                    if (from.sin_port == client_addrs[i].sin_port) {
                        player_idx = i;
                        break;
                    }
                }

                // Apply input to the correct player
                if (player_idx >= 0) {
                    if (strchr(buf, 'L')) players_x[player_idx] -= 1.0f;
                    if (strchr(buf, 'R')) players_x[player_idx] += 1.0f;
                }
            }
            // n == -1 with EAGAIN/EWOULDBLOCK means no more data – that's fine
        }

        // 3. Fixed‑step simulation & broadcast
        while (accumulator >= TICK_DURATION) {
            accumulator -= TICK_DURATION;
            // (Physics / game logic would go here – currently just positions already updated by input)

            // Send snapshot to all clients
            char msg[128];
            snprintf(msg, sizeof(msg), "P0:%.2f P1:%.2f", players_x[0], players_x[1]);
            for (int i = 0; i < 2; i++) {
                sendto(udp_fd, msg, (int)strlen(msg), 0,
                    (struct sockaddr*)&client_addrs[i], sizeof(client_addrs[i]));
            }
        }

        // 4. Small sleep to avoid 100% CPU (1 ms gives ~1000 Hz max poll rate)
        sleep_ms(1);
    }

    CLOSE_SOCKET(udp_fd);
    printf("Server finished.\n");
}


// ---------- Server entry (unchanged) ----------
static void server(void) {
    unsigned short client_udp_ports[2];
    if (tcp_handshake_server(client_udp_ports) != 0) {
        fprintf(stderr, "Handshake failed.\n");
        return;
    }
    udp_game_server(client_udp_ports);
}

// ---------- Client (SFML 3) – with prediction & interpolation ----------
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <string>
#include <algorithm>   // for std::min

static int tcp_handshake_client(int server_port, unsigned short* out_my_udp_port, int* out_player_id) {
    SOCKET tcp_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (tcp_fd < 0) { print_error("TCP socket"); return -1; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(tcp_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        print_error("TCP connect"); CLOSE_SOCKET(tcp_fd); return -1;
    }

    int player_id;
    unsigned short server_udp_port;
    if (recv(tcp_fd, (char*)&player_id, sizeof(player_id), 0) != sizeof(player_id) ||
        recv(tcp_fd, (char*)&server_udp_port, sizeof(server_udp_port), 0) != sizeof(server_udp_port)) {
        print_error("TCP recv"); CLOSE_SOCKET(tcp_fd); return -1;
    }

    SOCKET udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0) { print_error("UDP socket"); CLOSE_SOCKET(tcp_fd); return -1; }
    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = INADDR_ANY;
    my_addr.sin_port = 0;
    if (bind(udp_fd, (struct sockaddr*)&my_addr, sizeof(my_addr)) != 0) {
        print_error("UDP bind"); CLOSE_SOCKET(tcp_fd); CLOSE_SOCKET(udp_fd); return -1;
    }
    socklen_t len = sizeof(my_addr);
    getsockname(udp_fd, (struct sockaddr*)&my_addr, &len);
    unsigned short my_udp_port = ntohs(my_addr.sin_port);

    send(tcp_fd, (const char*)&my_udp_port, sizeof(my_udp_port), 0);
    CLOSE_SOCKET(tcp_fd);

    *out_my_udp_port = my_udp_port;
    *out_player_id = player_id;
    return (int)udp_fd;
}

static void client(int server_tcp_port) {
    unsigned short my_udp_port;
    int player_id;
    SOCKET udp_fd = tcp_handshake_client(server_tcp_port, &my_udp_port, &player_id);
    if (udp_fd < 0) return;

    set_nonblocking(udp_fd);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // SFML window
    sf::RenderWindow window(sf::VideoMode({ 800, 600 }), "Game Client");
    window.setFramerateLimit(60);

    sf::CircleShape local_shape(20);
    sf::CircleShape remote_shape(20);
    local_shape.setFillColor(player_id == 0 ? sf::Color::Red : sf::Color::Blue);
    remote_shape.setFillColor(player_id == 0 ? sf::Color::Blue : sf::Color::Red);

    sf::Font font;
    if (!font.openFromFile("bubbly.ttf")) {
        // font not found – game still works without text
    }
    sf::Text text(font);
    text.setCharacterSize(20);
    text.setFillColor(sf::Color::White);

    // ---- client‑side prediction & interpolation state ----
    float local_x = 0.0f;          // my predicted position
    float remote_x = 0.0f;        // other player's interpolated position
    int other_player = (player_id == 0) ? 1 : 0;

    float prev_remote_x = 0.0f, next_remote_x = 0.0f;
    sf::Clock snapshot_clock;     // time since last snapshot
    const float SNAPSHOT_INTERVAL = 1.0f / 60.0f;   // assume 60 Hz from server

    bool running = true;

    while (running && window.isOpen()) {
        // --- SFML events ---
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        // --- SEND INPUT EVERY FRAME (no throttle) ---
// --- SEND INPUT ONLY IF WINDOW HAS FOCUS ---
        if (window.hasFocus()) {
            std::string input;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A))
                input += 'L';
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right) ||
                sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D))
                input += 'R';

            if (!input.empty()) {
                sendto(udp_fd, input.c_str(), (int)input.size(), 0,
                    (struct sockaddr*)&server_addr, sizeof(server_addr));

                // Client‑side prediction: apply movement immediately
                if (input.find('L') != std::string::npos) local_x -= 1.0f;
                if (input.find('R') != std::string::npos) local_x += 1.0f;
            }
        }

        // --- RECEIVE SERVER STATE ---
        char buf[128];
        int n = recvfrom(udp_fd, buf, sizeof(buf) - 1, 0, NULL, NULL);
        if (n > 0) {
            buf[n] = '\0';
            float p0, p1;
            if (sscanf(buf, "P0:%f P1:%f", &p0, &p1) == 2) {
                // Update remote player interpolation targets
                prev_remote_x = next_remote_x;
                next_remote_x = (other_player == 0) ? p0 : p1;
                snapshot_clock.restart();

                // Reconciliation: snap local player to server's authoritative position
                float server_my_x = (player_id == 0) ? p0 : p1;
                local_x = server_my_x;   // (will be smoothly corrected later)
            }
        }

        // --- INTERPOLATE REMOTE PLAYER ---
        float elapsed = snapshot_clock.getElapsedTime().asSeconds();
        float t = std::min<float>(elapsed / SNAPSHOT_INTERVAL, 1.0f);
        remote_x = prev_remote_x + t * (next_remote_x - prev_remote_x);

        // --- RENDER ---
        window.clear();

        const float scale = 50.f;
        // Local player (predicted position)
        local_shape.setPosition({ local_x * scale + 400.f, 300.f });
        // Remote player (interpolated position)
        remote_shape.setPosition({ remote_x * scale + 400.f, 350.f });

        window.draw(local_shape);
        window.draw(remote_shape);

        // Display info
        char info[128];
        snprintf(info, sizeof(info), "Player %d  me: %.2f  other: %.2f", player_id, local_x, remote_x);
        text.setString(info);
        window.draw(text);

        window.display();
    }

    CLOSE_SOCKET(udp_fd);
}

// ---------- Main (unchanged) ----------
int main(int argc, char* argv[]) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    int ret = 0;
    if (argc > 1 && !strcmp(argv[1], "client")) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s client <server_tcp_port>\n", argv[0]);
            ret = -1;
            goto cleanup;
        }
        int tcp_port;
        sscanf(argv[2], "%d", &tcp_port);
        client(tcp_port);
    }
    else {
        server();
    }

cleanup:
#ifdef _WIN32
    WSACleanup();
#endif
    return ret;
}