// GameState.h
#pragma once
#include <SFML/Graphics.hpp>
#include <memory>

class IGameState {
public:
    virtual ~IGameState() = default;
    virtual void enter() = 0;
    virtual void exit() = 0;
    virtual void handleEvent(const sf::Event&) = 0;
    virtual void update(sf::Time dt) = 0;
    virtual void draw(sf::RenderWindow&) = 0;
};