#pragma once
#include <SFML/Graphics.hpp>
#include <unordered_map>
#include <array>
#include <vector>
#include <optional>
#include <network/NetTypes.h>   // AnimType

struct AnimationSet {
    std::unordered_map<AnimType, sf::Texture*> animMap;
    std::unordered_map<AnimType, std::array<std::vector<sf::IntRect>, 2>> animRects; // [0]=left, [1]=right
    std::unordered_map<AnimType, int> frameCounts;
    std::unordered_map<AnimType, float> animDurations;   // seconds per frame
    std::unordered_map<AnimType, bool> loops;
    std::unordered_map<AnimType, std::optional<AnimType>> nextAnim;
};

// Fills the set with player-specific data. Textures must outlive the AnimationSet.
void initPlayerAnimations(AnimationSet& animSet);