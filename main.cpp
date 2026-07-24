#include <network/client/Client.h>
#include <network/server/Server.h>
#include <cstring>

int main(int argc, char* argv[]) {
    if (argc > 1 && std::strcmp(argv[1], "client") == 0)
        run_client(13579);
    else
        run_server();
    return 0;
}