#pragma once
#include <cstdint>
#include <vector>
#include <SFML/Network/Packet.hpp>


// ---------- Quantisation ----------
constexpr float QUANT_SCALE = 100.0f;
inline int32_t quantise(float v) { return static_cast<int32_t>(v * QUANT_SCALE); }
inline float unquantise(int32_t q) { return static_cast<float>(q) / QUANT_SCALE; }

// ---------- Packet types ----------
enum class NetMsgType : uint8_t { SpawnEntity = 1, DestroyEntity = 2, FrameSnapshot = 3 };

// ---------- Entity structures ----------

// Server-side entity (includes state needed for game logic)
struct Entity {
    uint32_t id;
    float x, y;
    uint8_t animation = 0;
    uint16_t animStartTick = 0;   // server tick when animation changed
    // You might want a type here as well, but the server already knows it
};

// Snapshot entry sent in each frame (only dynamic data)
struct EntitySnapshot {
    uint32_t entityId;
    int32_t x_quant, y_quant;
    uint8_t animation;
    uint16_t animStartTick;       // client uses this to compute animation frame
    uint8_t flags;                // e.g., bit0 = facing right, bit1 = active, etc.
};

enum class EntityType : uint8_t
{
    Player = 0,
    Goblin = 1,
    Projectile = 2,
    NPC = 3,
    // ... add more as needed
};

enum class AnimType : uint8_t {
    Idle = 0,
    Walk = 1,
    Jump = 2,
    Attack = 3,
    // Add more animation types as needed
};

// Sent once when an entity enters the client's world (reliable)
struct SpawnMessage {
    //NetMsgType type = NetMsgType::SpawnEntity;
    uint32_t entityId;
    EntityType entityType;           // <<< tells the client WHAT to create
    float x, y;
    uint8_t animation = 0;
    // add any static data that never changes: max health, texture name hash, etc.
};

// Sent when an entity leaves (reliable)
struct DestroyMessage {
    //NetMsgType type = NetMsgType::DestroyEntity;
    uint32_t entityId;
};

// Full frame state (unreliable)
struct FrameSnapshot {
    //NetMsgType type = NetMsgType::FrameSnapshot;
    uint32_t frameNumber;                        // incremented each server tick
    std::vector<EntitySnapshot> entities;
};

// ---------- NetMsgType ----------
sf::Packet& operator<<(sf::Packet& p, NetMsgType t) {
    return p << static_cast<uint8_t>(t);
}
sf::Packet& operator>>(sf::Packet& p, NetMsgType& t) {
    uint8_t v;
    p >> v;
    t = static_cast<NetMsgType>(v);
    return p;
}

// ---------- EntityType ----------
sf::Packet& operator<<(sf::Packet& p, EntityType t) {
    return p << static_cast<uint8_t>(t);
}
sf::Packet& operator>>(sf::Packet& p, EntityType& t) {
    uint8_t v;
    p >> v;
    t = static_cast<EntityType>(v);
    return p;
}


// ---------- AnimType ----------
sf::Packet& operator<<(sf::Packet& p, AnimType t) {
    return p << static_cast<uint8_t>(t);
}
sf::Packet& operator>>(sf::Packet& p, AnimType& t) {
    uint8_t v;
    p >> v;
    t = static_cast<AnimType>(v);
    return p;
}


// ---------- EntitySnapshot ----------
sf::Packet& operator<<(sf::Packet& p, const EntitySnapshot& s) {
    return p << s.entityId << s.x_quant << s.y_quant
        << s.animation << s.animStartTick << s.flags;
}
sf::Packet& operator>>(sf::Packet& p, EntitySnapshot& s) {
    return p >> s.entityId >> s.x_quant >> s.y_quant
        >> s.animation >> s.animStartTick >> s.flags;
}

// ---------- SpawnMessage ----------
// operator<< – remove msg.type
sf::Packet& operator<<(sf::Packet& p, const SpawnMessage& msg) {
    return p << msg.entityId << msg.entityType << msg.x << msg.y << msg.animation;
}
// operator>> – remove msg.type
sf::Packet& operator>>(sf::Packet& p, SpawnMessage& msg) {
    return p >> msg.entityId >> msg.entityType >> msg.x >> msg.y >> msg.animation;
}

// ---------- DestroyMessage ----------
// operators without type
sf::Packet& operator<<(sf::Packet& p, const DestroyMessage& msg) {
    return p << msg.entityId;
}
sf::Packet& operator>>(sf::Packet& p, DestroyMessage& msg) {
    return p >> msg.entityId;
}

// ---------- FrameSnapshot ----------
// operators without type
sf::Packet& operator<<(sf::Packet& p, const FrameSnapshot& snap) {
    p << snap.frameNumber;
    p << static_cast<uint32_t>(snap.entities.size());
    for (const auto& e : snap.entities) p << e;
    return p;
}
sf::Packet& operator>>(sf::Packet& p, FrameSnapshot& snap) {
    p >> snap.frameNumber;
    uint32_t count; p >> count;
    snap.entities.resize(count);
    for (auto& e : snap.entities) p >> e;
    return p;
}