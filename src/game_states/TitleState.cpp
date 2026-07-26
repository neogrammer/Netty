#include "TitleState.h"
#include <mgmt/GameStateManager.h>
#include <res/Cfg.h>

TitleState::TitleState(sf::RenderWindow* win, ClientContext& ctx)
    : window(win), context(ctx)
    , titleText{ Cfg::fonts.get((int)Cfg::Fonts::Bubbly) }
	, promptText{ Cfg::fonts.get((int)Cfg::Fonts::Bubbly) }
{
    titleText.setString("My Game");
    titleText.setCharacterSize(60);
    titleText.setFillColor(sf::Color::White);
    titleText.setPosition({ 200, 150 });
    promptText.setString("Press Enter to start");
    promptText.setCharacterSize(30);
    promptText.setFillColor(sf::Color::Yellow);
    promptText.setPosition({ 250, 350 });
}

void TitleState::enter() {}
void TitleState::exit() {}

void TitleState::handleEvent(const sf::Event& event) {
    if (event.is<sf::Event::KeyPressed>() &&
        event.getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Enter) {
        // Notify server we're ready (send a simple UDP "READY" string)
        const char* ready = "READY";
        context.udpSocket->send(ready, 5, context.serverIp, context.serverPort);
        // The server will respond with LoadZone via TCP, which is handled centrally.
        // After that, the central processing will switch to OverworldState automatically.
    }
}

void TitleState::update(sf::Time dt) {
    (dt);
    // Nothing animated
}

void TitleState::draw(sf::RenderWindow& window) {
    window.clear(sf::Color::Black);
    window.draw(titleText);
    window.draw(promptText);
    window.display();
}