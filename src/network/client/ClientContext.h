#pragma once
#include <SFML/Network.hpp>
#include <memory>
#include <vector>
#include <network/NetTypes.h>  
class GameStateManager;

struct ClientContext {
    sf::UdpSocket* udpSocket{ nullptr };
    sf::TcpSocket* tcpSocket{ nullptr };
    sf::IpAddress   serverIp{0ui8, 0ui8, 0ui8, 0ui8};
    unsigned short  serverPort{ 0 };
    int             playerId{ -1 };
    uint32_t        myEntityId{ 0xFFFFFFFF };
    int             currentZone{ 1 };
    int             currentLevel{ -1 };
    GameStateManager* gsm{ nullptr };
    std::string latestInput;
    // Central message queues
    std::vector<SpawnMessage>   pendingSpawns;
    std::vector<DestroyMessage> pendingDestroys;
    sf::Packet                  latestSnapshot;
    bool                        hasSnapshot = false;
};
