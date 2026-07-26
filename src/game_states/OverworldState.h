#pragma once
#include "GameState.h"
#include <network/client/ClientContext.h>
#include <res/SceneResources.h>
#include <network/NetTypes.h>
#include <SFML/Graphics.hpp>
#include <unordered_map>
#include <entities/Entity.h>
#include <memory>

class OverworldState : public IGameState {
public:
    OverworldState(sf::RenderWindow* win, ClientContext& ctx,
        const std::unordered_map<EntityType, AnimationSet*>& animSets);

    void enter() override;
    void exit() override;
    void handleEvent(const sf::Event&) override;
    void update(sf::Time dt) override;
    void draw(sf::RenderWindow& window) override;
private:
    sf::RenderWindow* window;
    ClientContext& context;
    SceneResources zoneRes;                     // assets for current zone
    std::unique_ptr<sf::Sprite> mapSprite{ nullptr };                       // zone map texture
    std::unordered_map<uint32_t, ClientEntity> entities; // overworld entities (player icons, etc.)
    const std::unordered_map<EntityType, AnimationSet*>& entityAnimSets;

    struct {
        FrameSnapshot prev;
        FrameSnapshot curr;
        sf::Time      lastSnapTime;
        bool          hasPrev = false;
    } snapState;
    sf::Clock interpClock;
    const sf::Time tickDuration = sf::seconds(1.f / 60.f);
    float currentRenderTick = 0.f;
    void interpolateEntities(float renderTick);

    // bezier / movement data (stub)
};