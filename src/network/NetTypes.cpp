#include <network/NetTypes.h>


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
sf::Packet& operator<<(sf::Packet& p, const SpawnMessage& msg) {
    return p << msg.entityId << msg.entityType << msg.x << msg.y << msg.animation;
}
sf::Packet& operator>>(sf::Packet& p, SpawnMessage& msg) {
    return p >> msg.entityId >> msg.entityType >> msg.x >> msg.y >> msg.animation;
}

// ---------- DestroyMessage ----------
sf::Packet& operator<<(sf::Packet& p, const DestroyMessage& msg) {
    return p << msg.entityId;
}
sf::Packet& operator>>(sf::Packet& p, DestroyMessage& msg) {
    return p >> msg.entityId;
}

// ---------- FrameSnapshot ----------
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

sf::Packet& operator<<(sf::Packet& p, const AssignPlayerMessage& msg) {
    return p << msg.entityId;
}
sf::Packet& operator>>(sf::Packet& p, AssignPlayerMessage& msg) {
    return p >> msg.entityId;
}