#pragma once
#include "ResourceManager.h"
#include <SFML/Graphics.hpp>
#include <unordered_set>
#include <functional>

class LevelResources
{
public:
    ResourceManager<sf::Texture, int> textures{};
    ResourceManager<sf::Font, int> fonts{};

    void loadForLevel(int levelNum);
    void clear();

private:
    // These sets store the IDs (ints) that are currently loaded.
    std::unordered_set<int> loadedTextureIDs;
    std::unordered_set<int> loadedFontIDs;

    // Sync a single resource manager with the required ID set.
    template<typename ResType>
    void syncManager(
        ResourceManager<ResType, int>& manager,
        std::unordered_set<int>& loadedSet,
        const std::unordered_set<int>& neededSet,
        const std::function<std::string(int)>& pathProvider);
};

#include "tpl/LevelResources.tpl"