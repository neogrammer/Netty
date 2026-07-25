#pragma once
#include <memory>
#include <SFML/Graphics.hpp>
#include <unordered_map>
#include <NetTypes.h>
#include <entities/animation/AnimationSet.h>

struct ClientEntity {
    std::unique_ptr<sf::Sprite> sprite;
    AnimationSet* animSet = nullptr;
    AnimType currentAnim = AnimType::Idle;
    uint32_t animStartTick = 0;
    float x = 0.f, y = 0.f;

    ClientEntity() = default;
    ClientEntity(const ClientEntity&) = delete;
    ClientEntity& operator=(const ClientEntity&) = delete;
    ClientEntity(ClientEntity&&) = default;
    ClientEntity& operator=(ClientEntity&&) = default;
};

// Creates a client entity and initialises its sprite from the given animation set map.
std::pair<bool, ClientEntity> createClientEntity(
    const std::unordered_map<EntityType, AnimationSet*>& entityAnimSets,
    EntityType type, float x, float y);