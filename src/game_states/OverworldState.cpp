#include "OverworldState.h"
#include <entities/Entity.h>
#include <res/Cfg.h>
#include <game_states/PlayState.h>


OverworldState::OverworldState(sf::RenderWindow* win, ClientContext& ctx,
    const std::unordered_map<EntityType, AnimationSet*>& animSets)
    : window(win), context(ctx), entityAnimSets(animSets), mapSprite{nullptr} {}

void OverworldState::enter() {
    int zone = context.currentZone;
    if (zone <= 0) {
        // No valid zone yet – maybe draw a placeholder
        printf("[OverworldState] Invalid zone %d – waiting for server\n", zone);
        return;
    }
    // Load zone assets
    zoneRes.loadForScene(zone, false);
  
    // Create the map sprite once the texture is available
    auto zoneMapId = static_cast<int>(Cfg::Textures::Zone1_Map);
    if (zoneRes.textures.isLoaded(zoneMapId)) {
        auto& tex = zoneRes.textures.get(zoneMapId);
        mapSprite = std::make_unique<sf::Sprite>(tex);
    }
    else {
        printf("[OverworldState] Zone1_Map texture not loaded!\n");
    }
}

void OverworldState::exit() {
    entities.clear();
    // zoneRes will stay loaded until next zone change (lazy unload)
}

void OverworldState::handleEvent(const sf::Event& event) {
    // movement input will be sent via central input handler
}

void OverworldState::update(sf::Time dt) {

    // Client‑side prediction: apply own input immediately for smooth movement
    if (!context.latestInput.empty() && context.myEntityId != 0xFFFFFFFF) {
        auto it = entities.find(context.myEntityId);
        if (it != entities.end()) {
            float step = 300.0f * (1.f / 60.f);
            if (context.latestInput.find('L') != std::string::npos)
                it->second.x -= step;
            if (context.latestInput.find('R') != std::string::npos)
                it->second.x += step;
        }
    }
    // Process pending spawns
    for (auto& msg : context.pendingSpawns) {
        auto [ok, ent] = createClientEntity(entityAnimSets, msg.entityType, msg.x, msg.y);
        if (ok) {
            ent.currentAnim = static_cast<AnimType>(msg.animation);
            ent.animStartTick = msg.animStartTick;   // <-- set from spawn
            entities[msg.entityId] = std::move(ent);
        }
    }
    context.pendingSpawns.clear();

    // Process pending destroys
    for (auto& msg : context.pendingDestroys) {
        entities.erase(msg.entityId);
        if (context.myEntityId == msg.entityId)
            context.myEntityId = 0xFFFFFFFF;
    }
    context.pendingDestroys.clear();

}

void OverworldState::draw(sf::RenderWindow& window) {
    window.clear(sf::Color(50, 50, 50));   // dark fallback background

    if (mapSprite) {
        window.draw(*mapSprite);
    }

    for (auto& [id, ent] : entities) {
        if (ent.sprite) {
            ent.sprite->setPosition({ ent.x, ent.y });
            window.draw(*ent.sprite);
        }
    }

    window.display();
}

void OverworldState::interpolateEntities(float renderTick) {
    if (!snapState.hasPrev) return;

    float t = 0.f;
    if (snapState.curr.frameNumber != snapState.prev.frameNumber) {
        t = (renderTick - snapState.prev.frameNumber) /
            (float)(snapState.curr.frameNumber - snapState.prev.frameNumber);
    }
    t = std::min(t, 1.0f);

    for (const auto& snapEnt : snapState.curr.entities) {
        auto it = entities.find(snapEnt.entityId);
        if (it == entities.end()) continue;

        float x = unquantise(snapEnt.x_quant);
        float y = unquantise(snapEnt.y_quant);
        auto prevIt = std::find_if(snapState.prev.entities.begin(), snapState.prev.entities.end(),
            [&](const EntitySnapshot& s) { return s.entityId == snapEnt.entityId; });
        if (prevIt != snapState.prev.entities.end()) {
            float prevX = unquantise(prevIt->x_quant);
            float prevY = unquantise(prevIt->y_quant);
            x = prevX + t * (x - prevX);
            y = prevY + t * (y - prevY);
        }
        it->second.x = x;
        it->second.y = y;

        AnimType newAnim = static_cast<AnimType>(snapEnt.animation);
        if (newAnim != it->second.currentAnim) {
            it->second.currentAnim = newAnim;
            it->second.animStartTick = snapEnt.animStartTick;
        }
    }
}