#pragma once
#include <cstdint>
#include <vector>
#include <SFML/Network/Packet.hpp>


// ---------- Quantisation ----------

 constexpr float QUANT_SCALE = 100.0f;

 inline int32_t quantise(float v) { return static_cast<int32_t>(v * QUANT_SCALE); }
 inline float unquantise(int32_t q) { return static_cast<float>(q) / QUANT_SCALE; }

// ---------- Packet types ----------
enum class NetMsgType : uint8_t { SpawnEntity = 1, DestroyEntity = 2, FrameSnapshot = 3, AssignPlayerEntity = 4 };


// ---------- Entity structures ----------
enum class EntityType : uint8_t {
    Player = 0,
    Goblin = 1,
    Projectile = 2,
    NPC = 3,
};

// Server-side entity (includes state needed for game logic)
struct Entity {
    uint32_t id = 0;
    EntityType type = EntityType::NPC;
    float x = 0.f, y = 0.f;
    uint8_t animation = 0;
    uint32_t animStartTick = 0;
};

// Snapshot entry sent in each frame (only dynamic data)
struct EntitySnapshot {
    uint32_t entityId;
    int32_t x_quant, y_quant;
    uint8_t animation;
    uint32_t animStartTick;       // client uses this to compute animation frame
    uint8_t flags;                // e.g., bit0 = facing right
};



enum class AnimType : uint8_t {
    Idle = 0,
    Walk = 1,
    Jump = 2,
    Attack = 3,
};

// Sent once when an entity enters the client's world (reliable)
struct SpawnMessage {
    uint32_t entityId;
    EntityType entityType;
    float x, y;
    uint8_t animation = 0;
};

// Sent when an entity leaves (reliable)
struct DestroyMessage {
    uint32_t entityId;
};

struct AssignPlayerMessage {
    uint32_t entityId;
};

// Full frame state (unreliable)
struct FrameSnapshot {
    uint32_t frameNumber;
    std::vector<EntitySnapshot> entities;
};

// ---------- NetMsgType ----------
sf::Packet& operator<<(sf::Packet& p, NetMsgType t); 
sf::Packet& operator>>(sf::Packet& p, NetMsgType& t);
// ---------- EntityType ----------
sf::Packet& operator<<(sf::Packet& p, EntityType t); 
sf::Packet& operator>>(sf::Packet& p, EntityType& t); 

// ---------- AnimType ----------
sf::Packet& operator<<(sf::Packet& p, AnimType t); 
sf::Packet& operator>>(sf::Packet& p, AnimType& t);
// ---------- EntitySnapshot ----------
sf::Packet& operator<<(sf::Packet& p, const EntitySnapshot& s);
sf::Packet& operator>>(sf::Packet& p, EntitySnapshot& s);
// ---------- SpawnMessage ----------
sf::Packet& operator<<(sf::Packet& p, const SpawnMessage& msg);
sf::Packet& operator>>(sf::Packet& p, SpawnMessage& msg); 
// ---------- DestroyMessage ----------
sf::Packet& operator<<(sf::Packet& p, const DestroyMessage& msg); 
sf::Packet& operator>>(sf::Packet& p, DestroyMessage& msg); 
// ---------- FrameSnapshot ----------
sf::Packet& operator<<(sf::Packet& p, const FrameSnapshot& snap);
sf::Packet& operator>>(sf::Packet& p, FrameSnapshot& snap); 

sf::Packet& operator<<(sf::Packet& p, const AssignPlayerMessage& msg); 
sf::Packet& operator>>(sf::Packet& p, AssignPlayerMessage& msg);