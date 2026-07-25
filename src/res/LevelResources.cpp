#include "LevelResources.h"
#include <string>
#include <stdexcept>
#include <res/Cfg.h>

static std::string texturePath(Cfg::Textures id)
{
    // Map from ID to filepath – you could read from a config file.
    switch (id) {
    case Cfg::Textures::Level1Background: return "assets/textures/level1_bg.png"; // TODO add file
	case Cfg::Textures::Level1Middle: return "assets/textures/level1_mid.png"; // TODO add file
        // … add all known IDs
    default: throw std::runtime_error("Unknown texture ID");
    }
}

static std::string fontPath(Cfg::Fonts id)
{
    // Map from ID to filepath – you could read from a config file.
    switch (id) {
    case Cfg::Fonts::Bubbly: return "assets/fonts/bubbly.ttf";
        // … add all known IDs
    default: throw std::runtime_error("Unknown font ID");
    }
}

std::unordered_set<int> getRequiredTextureIDs(int levelNum)
{
    std::unordered_set<int> ids;
    switch (levelNum) {
    case 1: ids = { (int)Cfg::Textures::Level1Background, (int)Cfg::Textures::Level1Middle }; break;
    case 2: ids = { (int)Cfg::Textures::Level1Background, (int)Cfg::Textures::Level1Middle, (int)Cfg::Textures::Count }; break;
        // …
    default: break;
    }
    return ids;
}

// Optional: if you want per-level fonts
std::unordered_set<int> getRequiredFontIDs(int levelNum)
{
    return {};  // usually empty
}


void LevelResources::loadForLevel(int levelNum)
{
    const auto neededTex = getRequiredTextureIDs(levelNum);
    const auto neededFnt = getRequiredFontIDs(levelNum);

    syncManager<sf::Texture>(textures, loadedTextureIDs, neededTex,
        [](int id) -> std::string { return texturePath(static_cast<Cfg::Textures>(id)); });
    syncManager<sf::Font>(fonts, loadedFontIDs, neededFnt,
        [](int id) -> std::string { return fontPath(static_cast<Cfg::Fonts>(id)); });
}

void LevelResources::clear()
{
    loadedTextureIDs.clear();
    loadedFontIDs.clear();
    textures.clearAll();
    fonts.clearAll();
}
