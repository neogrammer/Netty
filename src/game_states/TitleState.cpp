#include "TitleState.h"
#include <mgmt/GameStateManager.h>

TitleState::TitleState(sf::RenderWindow* win, GameStateManager* manager)
    : window(win), gsm(manager)
{
    font.openFromFile("assets/fonts/bubbly.ttf"); // adjust path
}

void TitleState::enter() {}
void TitleState::exit() {}

void TitleState::handleEvent(const sf::Event& event) {
    if (event.is<sf::Event::KeyPressed>() &&
        event.getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Enter) {
        gsm->switchTo("play");
    }
}

void TitleState::update(sf::Time dt) {
    // Nothing animated
}

void TitleState::draw(sf::RenderWindow& window) {
    window.clear(sf::Color::Black);
    sf::Text titleText{ font };
    sf::Text promptText{ font };    
    titleText.setString("My Game");
    titleText.setCharacterSize(60);
    titleText.setFillColor(sf::Color::White);
    titleText.setPosition({ 200, 150 });
    promptText.setString("Press Enter to start");
    promptText.setCharacterSize(30);
    promptText.setFillColor(sf::Color::Yellow);
    promptText.setPosition({ 250, 350 });

    window.draw(titleText);
    window.draw(promptText);
    window.display();
}