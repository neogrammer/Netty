#include <res/Cfg.h>

ResourceManager<sf::Texture, int> Cfg::textures = {};
ResourceManager<sf::Font, int> Cfg::fonts = {};


void Cfg::Initialize()
{
    initTextures();
    initFonts();
}


void Cfg::initTextures()
{
    textures.load((int)Textures::Player_Idle, "assets/textures/player/idle.png");
    textures.load((int)Textures::Player_Walk, "assets/textures/player/walk.png");
}


void Cfg::initFonts()
{
    fonts.load((int)Fonts::Bubbly, "assets/fonts/bubbly.ttf");
}