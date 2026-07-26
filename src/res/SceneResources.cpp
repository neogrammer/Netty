#include "SceneResources.h"
#include <string>
#include <stdexcept>
#include <res/Cfg.h>

static std::string texturePath(Cfg::Textures id)
{
    // Map from ID to filepath – you could read from a config file.
    switch (id) {
        // ---------- overworld zones (1000-1999) ----------
    case Cfg::Textures::Zone1_Map:      return "assets/textures/zones/Zone1_Map.jpg";
    case Cfg::Textures::Zone1_Icons:    return "assets/textures/zones/Zone1_Icons.png";
        // ---------- levels (100-199) ----------
    case Cfg::Textures::L1_BgFar:       return "assets/textures/levels/Level1/L1_BgFar.png";
    case Cfg::Textures::L1_BgMid_Far:   return "assets/textures/levels/Level1/L1_BgMid_Far.png";
    case Cfg::Textures::L1_BgMid_Mid:   return "assets/textures/levels/Level1/L1_BgMid_Mid.png";
    case Cfg::Textures::L1_BgMid_Near:  return "assets/textures/levels/Level1/L1_BgMid_Near.png";
    case Cfg::Textures::L1_Foreground:  return "assets/textures/levels/Level1/L1_Foreground.png";
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

// ---------- required ID sets (scene‑specific) ----------
static std::unordered_set<int> requiredTextureIDs(int sceneId, bool isLevel = true)
{
    using T = Cfg::Textures;
    std::unordered_set<int> ids;
    // Overworld zones
    if (!isLevel)
    {
        if (sceneId == 0)
        {  // level 0 (test)
            ids = { (int)T::None };
        }
        else if (sceneId == 1) {  // overworld zone 1
            ids = { (int)T::Zone1_Map, (int)T::Zone1_Icons };
        }
        else
        {
                ids = { (int)T::None };      
        }
    }
    // Levels
    else if (isLevel)
    {
		if (sceneId == 0)
		{  // level 0 (test)
            ids = { (int)T::None };
		}
		else if (sceneId == 1)
        {  // level 1
        ids = { (int)T::L1_BgFar,
                    (int)T::L1_BgMid_Far,
                    (int)T::L1_BgMid_Mid,
                    (int)T::L1_BgMid_Near,
                    (int)T::L1_Foreground };
        }
        // … add more levels here
        else
        {
            ids = { (int)T::None };
        }
    } 

    return ids;
}

static std::unordered_set<int> requiredFontIDs(int id, bool isLevel = true)
{
    return {};   // usually empty – per‑level fonts are rare
}


void SceneResources::loadForScene(int sceneId, bool isLevel)
{
    const auto neededTex = requiredTextureIDs(sceneId, isLevel);
    const auto neededFnt = requiredFontIDs(sceneId, isLevel);

    syncManager<sf::Texture>(textures, loadedTextureIDs, neededTex,
        [](int id) -> std::string { return texturePath(static_cast<Cfg::Textures>(id)); });
    syncManager<sf::Font>(fonts, loadedFontIDs, neededFnt,
        [](int id) -> std::string { return fontPath(static_cast<Cfg::Fonts>(id)); });
}

void SceneResources::clear()
{
    loadedTextureIDs.clear();
    loadedFontIDs.clear();
    textures.clearAll();
    fonts.clearAll();
}
