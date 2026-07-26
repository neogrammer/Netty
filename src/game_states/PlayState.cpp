#include "PlayState.h"
#include <cstdio>
#include <algorithm>
#include <cstring>

PlayState::PlayState(sf::RenderWindow* win, ClientContext& ctx,
    const std::unordered_map<EntityType, AnimationSet*>& animSets)
    : window(win), context(ctx), entityAnimSets(animSets) {}
    

void PlayState::enter() {
    // Assets are already loaded by central TCP handler before switching
    // Snapshot interpolation starts fresh
    levelRes.loadForScene(context.currentLevel, true);
    snapState.hasPrev = false;
    interpClock.restart();
    printf("[PlayState] Entered level %d\n", context.currentLevel);


}

void PlayState::exit() {
   // printf("[PlayState] Exiting\n");
    entities.clear();
}

void PlayState::handleEvent(const sf::Event& event) {
    // Not used for gameplay input
}



void PlayState::update(sf::Time dt) {

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

    // Process snapshot if new one arrived
    if (context.hasSnapshot) {
        snapState.prev = std::move(snapState.curr);
        context.latestSnapshot >> snapState.curr;
        printf("[Client %d] Got snapshot frame %u with %zu entities\n",
            context.playerId, snapState.curr.frameNumber, snapState.curr.entities.size());
        if (!snapState.curr.entities.empty()) {
            for (auto& e : snapState.curr.entities) {
                printf("   entity %u: x_quant=%d y_quant=%d anim=%u tick=%u\n",
                    e.entityId, e.x_quant, e.y_quant, e.animation, e.animStartTick);
            }
        }
        snapState.lastSnapTime = interpClock.getElapsedTime();
        snapState.hasPrev = true;
        context.hasSnapshot = false;
    }


    // Input sending is handled centrally in run_client, so nothing here.
    // Entities are updated by snapshots received centrally and stored in our entities map.
    // Interpolation:
    currentRenderTick = 0.f;
    if (snapState.hasPrev) {
        sf::Time now = interpClock.getElapsedTime();
        float t = ((now - snapState.lastSnapTime).asSeconds()) / tickDuration.asSeconds();
        t = std::min(t, 1.0f);
        uint32_t prevTick = snapState.prev.frameNumber;
        uint32_t currTick = snapState.curr.frameNumber;
        currentRenderTick = prevTick + t * (currTick - prevTick);
        interpolateEntities(currentRenderTick);
    }
}

void PlayState::draw(sf::RenderWindow& window) {
  
    window.clear();

	// draw parallax background here

    // draw part of map that is on same level as player, the ground

    // draw the game entities
    for (auto& [id, ent] : entities) {
        if (!ent.sprite) continue;

        int frameIdx = 0;
        if (ent.animSet) {
            AnimationSet& anim = *ent.animSet;
            AnimType cur = ent.currentAnim;

            // Calculate animation frame
            float elapsedTicks = std::max(0.f, currentRenderTick - ent.animStartTick);
            float timeSec = elapsedTicks * tickDuration.asSeconds();
            float durPerFrame = anim.animDurations[cur];
            int totalFrames = anim.frameCounts[cur];
            if (totalFrames > 0) {
                int rawFrame = static_cast<int>(timeSec / durPerFrame);
                frameIdx = anim.loops[cur] ? (rawFrame % totalFrames)
                    : std::min(rawFrame, totalFrames - 1);
            }

            // Determine facing (flags bit0)
            uint8_t facing = 0;
            // Look up the entity in the current snapshot (if available) to get flags
            if (snapState.hasPrev) {
                auto snapIt = std::find_if(snapState.curr.entities.begin(),
                    snapState.curr.entities.end(),
                    [&](const EntitySnapshot& s) { return s.entityId == id; });
                if (snapIt != snapState.curr.entities.end())
                    facing = snapIt->flags & 1;
            }

            // Apply the correct texture and texture rect
            if (anim.animMap.count(cur)) {
                sf::Texture* tex = anim.animMap[cur];
                if (tex) ent.sprite->setTexture(*tex);
                if (anim.animRects.count(cur) &&
                    anim.animRects[cur][facing].size() > static_cast<size_t>(frameIdx)) {
                    sf::IntRect rect = anim.animRects[cur][facing][frameIdx];
                    ent.sprite->setTextureRect(rect);
                }
            }
        }

        ent.sprite->setPosition({ ent.x, ent.y });
        window.draw(*ent.sprite);
    }
	// draw any info overlays like damage
	// draw foreground elements like trees, walls, etc
	// draw UI elements like health bars, score, etc.
    window.display();
}

// ---------- Private helpers (identical to old code) ----------
void PlayState::interpolateEntities(float renderTick) {

    printf("[Client %d] Interpolating with currentRenderTick=%.2f\n",
        context.playerId, renderTick);

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
        it->second.animStartTick = snapEnt.animStartTick;
    }
}