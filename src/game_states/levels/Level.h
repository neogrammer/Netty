// Level.h
#pragma once
#include <network/NetTypes.h>
#include <vector>
#include <string>
#include <unordered_map>

class Level {
public:
    std::vector<Entity> allEntities;
    std::unordered_map<uint32_t, size_t> entityIndex;   // entityId → index in allEntities

    void addEntity(Entity e) {
        size_t idx = allEntities.size();
        allEntities.push_back(e);
        entityIndex[e.id] = idx;
    }

    Entity* getEntity(uint32_t id) {
        auto it = entityIndex.find(id);
        return (it != entityIndex.end()) ? &allEntities[it->second] : nullptr;
    }

    // future: load from file
    void loadDefault() {
        // nothing special – entities will be added by the server
    }
};