// GameStateManager.h
#pragma once
#include <game_states/GameState.h>
#include <network/client/ClientContext.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <game_states/PlayState.h>
#include <game_states/TitleState.h>
#include <game_states/OverworldState.h>


class GameStateManager {
public:
    explicit GameStateManager(ClientContext& ctx) : context(ctx) {}


    void registerState(const std::string& name, std::unique_ptr<IGameState> state);
    void switchTo(const std::string& name);
    void handleEvent(const sf::Event& e);
    void update(sf::Time dt);
    void draw(sf::RenderWindow& window);

    ClientContext& getContext() { return context; }
private:
    ClientContext& context;
    std::unordered_map<std::string, std::unique_ptr<IGameState>> states;
    IGameState* currentState = nullptr;
};