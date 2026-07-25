#ifndef CFG_H__
#define CFG_H__

#include <SFML/Graphics.hpp>
#include <res/ResourceManager.h>
#include <vector>
#include <variant>
#include <utility>


struct Cfg
{
	Cfg() = delete;
	Cfg(const Cfg&) = delete;
	Cfg& operator=(const Cfg&) = delete;


	//globals 
	//static sol::state lua; // globals are bad, but we'll use it for simpler implementation
	static void Initialize();

	// Resource Enums 
	enum class Textures : int { 
		Player_Idle, Player_Walk,
		Level1Background, Level1Middle, Count, None };
	enum class Fonts : int { Bubbly, Count, None };


	// resource buckets for each type of resource
	static ResourceManager<sf::Texture, int> textures;
	static ResourceManager<sf::Font, int> fonts;

private:
	// initalize the resources for the entire game
	static void initFonts();
	static void initTextures();
};


#endif