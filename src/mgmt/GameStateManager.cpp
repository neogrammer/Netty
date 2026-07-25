#include "GameStateManager.h"
#include <stdexcept>

void GameStateManager::registerState(const std::string& name, std::unique_ptr<IGameState> state) {
    states[name] = std::move(state);
}

void GameStateManager::switchTo(const std::string& name) {
    auto it = states.find(name);
    if (it == states.end())
        throw std::runtime_error("State not found: " + name);

    if (currentState)
        currentState->exit();
    currentState = it->second.get();
    currentState->enter();
}

void GameStateManager::handleEvent(const sf::Event& e) {
    if (currentState)
        currentState->handleEvent(e);
}

void GameStateManager::update(sf::Time dt) {
    if (currentState)
        currentState->update(dt);
}

void GameStateManager::draw(sf::RenderWindow& window) {
    if (currentState)
        currentState->draw(window);
}