#include "Entity.h"

std::pair<bool, ClientEntity> createClientEntity(
    const std::unordered_map<EntityType, AnimationSet*>& entityAnimSets,
    EntityType type, float x, float y)
{
    ClientEntity ent;
    ent.x = x;
    ent.y = y;
    ent.currentAnim = AnimType::Idle;
    ent.animStartTick = 0;

    auto animIt = entityAnimSets.find(type);
    if (animIt == entityAnimSets.end())
        return { false, ClientEntity{} };

    ent.animSet = animIt->second;

    // Use the idle texture as the initial sprite texture
    auto texIt = ent.animSet->animMap.find(AnimType::Idle);
    if (texIt != ent.animSet->animMap.end() && texIt->second)
        ent.sprite = std::make_unique<sf::Sprite>(*texIt->second);

    // Set the initial texture rect to the first idle frame (facing right)
    if (ent.animSet) {
        auto& anim = *ent.animSet;
        auto cur = AnimType::Idle;
        if (anim.animRects.count(cur) && !anim.animRects[cur][1].empty()) { // 1 = right
            ent.sprite->setTextureRect(anim.animRects[cur][1][0]);
        }
    }

    return { true, std::move(ent) };
}