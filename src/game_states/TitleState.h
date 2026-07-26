// TitleState.h
#pragma once
#include <game_states/GameState.h>
#include <network/NetTypes.h>
#include <network/client/ClientContext.h>
#include <entities/Entity.h>
#include <entities/animation/AnimationSet.h>
#include <SFML/Network.hpp>
#include <unordered_map>
#include <SFML/Graphics.hpp>
#include <functional>

class GameStateManager; // forward decl

class TitleState : public IGameState {
public:
    TitleState(sf::RenderWindow* win, ClientContext& ctx);
    void enter() override;
    void exit() override;
    void handleEvent(const sf::Event& event) override;
    void update(sf::Time dt) override;
    void draw(sf::RenderWindow& window) override;
private:
    sf::RenderWindow* window;
    ClientContext& context;
    sf::Text titleText, promptText;
};