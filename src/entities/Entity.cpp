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

    return { true, std::move(ent) };
}