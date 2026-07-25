#include <stdexcept> //runtime_error
#include <utility> //forward
#include <SFML/Graphics.hpp>

template<typename RESOURCE,typename IDENTIFIER>
template<typename ... Args>
void ResourceManager<RESOURCE,IDENTIFIER>::load(const IDENTIFIER& id,Args&& ... args)
{
    std::unique_ptr<RESOURCE> ptr(new RESOURCE);
    if(not ptr->loadFromFile(std::forward<Args>(args)...))
        throw std::runtime_error("Impossible to load file");
    _map.emplace(id,std::move(ptr));
}

template<typename RESOURCE,typename IDENTIFIER>
RESOURCE& ResourceManager<RESOURCE,IDENTIFIER>::get(const IDENTIFIER& id)const
{
    return *_map.at(id);
}

template<typename RESOURCE,typename IDENTIFIER>
void ResourceManager<RESOURCE,IDENTIFIER>::unload(const IDENTIFIER& id)
{
        auto it = _map.find(id);
        if (it != _map.end())
            _map.erase(it);
}


template<typename RESOURCE,typename IDENTIFIER>
bool ResourceManager<RESOURCE,IDENTIFIER>::isLoaded(const IDENTIFIER& id) const
{
    return _map.find(id) != _map.end();
}

//sf::font special case
template<typename IDENTIFIER>
template <typename... Args>
void ResourceManager<sf::Font,IDENTIFIER>::load(const IDENTIFIER& id, Args&&... args)
{
    std::unique_ptr<sf::Font> ptr = std::make_unique<sf::Font>();
    if(!ptr->openFromFile(std::forward<Args>(args)...))
        throw std::runtime_error("impossible to load file");
    _map.emplace(id,std::move(ptr));
};

template<typename IDENTIFIER>
sf::Font& ResourceManager<sf::Font,IDENTIFIER>::get(const IDENTIFIER& id) const
{
    return *_map.at(id);
}

template<typename IDENTIFIER>
void ResourceManager<sf::Font,IDENTIFIER>::unload(const IDENTIFIER& id)
{
        auto it = _map.find(id);
        if (it != _map.end())
            _map.erase(it);
}


template<typename IDENTIFIER>
bool ResourceManager<sf::Font,IDENTIFIER>::isLoaded(const IDENTIFIER& id) const
{
    return _map.find(id) != _map.end();
}
