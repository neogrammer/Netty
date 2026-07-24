#include "AnimationSet.h"

void initPlayerAnimations(AnimationSet& animSet, sf::Texture& idleTex, sf::Texture& walkTex) 
{
    // ----- Idle -----
    animSet.animMap[AnimType::Idle] = &idleTex;
    animSet.frameCounts[AnimType::Idle] = 8;
    animSet.animDurations[AnimType::Idle] = 0.1f;
    animSet.loops[AnimType::Idle] = true;
    animSet.nextAnim[AnimType::Idle] = std::nullopt;

    animSet.animRects[AnimType::Idle][0] = {   // left
        sf::IntRect({0, 128}, {64, 128}),   sf::IntRect({64, 128}, {64, 128}),
        sf::IntRect({128, 128}, {64, 128}), sf::IntRect({192, 128}, {64, 128}),
        sf::IntRect({256, 128}, {64, 128}), sf::IntRect({320, 128}, {64, 128}),
        sf::IntRect({384, 128}, {64, 128}), sf::IntRect({448, 128}, {64, 128})
    };
    animSet.animRects[AnimType::Idle][1] = {   // right
        sf::IntRect({0, 256}, {64, 128}),   sf::IntRect({64, 256}, {64, 128}),
        sf::IntRect({128, 256}, {64, 128}), sf::IntRect({192, 256}, {64, 128}),
        sf::IntRect({256, 256}, {64, 128}), sf::IntRect({320, 256}, {64, 128}),
        sf::IntRect({384, 256}, {64, 128}), sf::IntRect({448, 256}, {64, 128})
    };

    // ----- Walk -----
    animSet.animMap[AnimType::Walk] = &walkTex;
    animSet.frameCounts[AnimType::Walk] = 10;
    animSet.animDurations[AnimType::Walk] = 0.08f;
    animSet.loops[AnimType::Walk] = true;
    animSet.nextAnim[AnimType::Walk] = std::nullopt;

    animSet.animRects[AnimType::Walk][0] = {
        sf::IntRect({0, 128}, {64, 128}),    sf::IntRect({64, 128}, {64, 128}),
        sf::IntRect({128, 128}, {64, 128}),  sf::IntRect({192, 128}, {64, 128}),
        sf::IntRect({256, 128}, {64, 128}),  sf::IntRect({320, 128}, {64, 128}),
        sf::IntRect({384, 128}, {64, 128}),  sf::IntRect({448, 128}, {64, 128}),
        sf::IntRect({512, 128}, {64, 128}),  sf::IntRect({576, 128}, {64, 128})
    };
    animSet.animRects[AnimType::Walk][1] = {
        sf::IntRect({0, 256}, {64, 128}),    sf::IntRect({64, 256}, {64, 128}),
        sf::IntRect({128, 256}, {64, 128}),  sf::IntRect({192, 256}, {64, 128}),
        sf::IntRect({256, 256}, {64, 128}),  sf::IntRect({320, 256}, {64, 128}),
        sf::IntRect({384, 256}, {64, 128}),  sf::IntRect({448, 256}, {64, 128}),
        sf::IntRect({512, 256}, {64, 128}),  sf::IntRect({576, 256}, {64, 128})
    };
}

