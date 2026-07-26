#ifndef RESOURCEMANAGER_H__
#define RESOURCEMANAGER_H__

#include <unordered_map> //unordered_map
#include <string> //string
#include <memory> //unique_ptr

#include <SFML/Graphics.hpp>


template<typename RESOURCE, typename IDENTIFIER = int>
struct ResourceManager
{
    ResourceManager() = default;

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;


    template<typename... Args>
    void load(const IDENTIFIER& id, Args&&... args);

    RESOURCE& get(const IDENTIFIER& id)const;
    void unload(const IDENTIFIER& id);
    bool isLoaded(const IDENTIFIER& id) const;
    inline void clearAll() { _map.clear(); }

    std::size_t size() const { return _map.size(); }
private:
    std::unordered_map<IDENTIFIER, std::unique_ptr<RESOURCE>> _map;
};

template<typename IDENTIFIER>
struct ResourceManager<sf::Font, IDENTIFIER>
{
    ResourceManager() = default;

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;


    
    template<typename ... Args>
    void load(const IDENTIFIER& id, Args&&... args);

    sf::Font& get(const IDENTIFIER& id) const;
    void unload(const IDENTIFIER& id);
    bool isLoaded(const IDENTIFIER& id) const;
    inline void clearAll() { _map.clear(); }

    std::size_t size() const { return _map.size(); }
private:
    std::unordered_map<IDENTIFIER, std::unique_ptr<sf::Font>> _map;
};

#include "tpl/ResourceManager.tpl"
#endif
