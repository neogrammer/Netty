#include <stdexcept> //runtime_error
#include <utility> //forward
#include <string>
#include <SFML/Graphics.hpp>

template<typename ResType>
void SceneResources::syncManager(
    ResourceManager<ResType, int>& manager,
    std::unordered_set<int>& loadedSet,
    const std::unordered_set<int>& neededSet,
    const std::function<std::string(int)>& pathProvider)
{
    // 1. Unload IDs that are no longer needed
    for (auto it = loadedSet.begin(); it != loadedSet.end(); ) {
        if (neededSet.find(*it) == neededSet.end()) {
            manager.unload(*it);
            it = loadedSet.erase(it);
        } else {
            ++it;
        }
    }

    // 2. Load missing IDs
    for (int id : neededSet) {
        if (loadedSet.find(id) == loadedSet.end()) {
            manager.load(id, pathProvider(id));
            loadedSet.insert(id);
        }
    }
}