#pragma once
// Renderer.h
// Draws everything: the world, the agents with their goal + plan labels,
// the HUD, the event log and the help overlay. Part B of the demo.
// The renderer only READS from the simulation - all game logic is in sim/.

#include <SFML/Graphics.hpp>
#include "../contract/Contract.h"

class Renderer {
public:
    // loads the font. returns false if assets/DejaVuSans.ttf is missing
    bool init();

    void draw(sf::RenderWindow& window, const goap::Simulation& sim,
              bool paused, bool showLabels, bool showHelp, float runTime);

private:
    sf::Font m_font;

    // little helper so making text is a one-liner
    sf::Text makeText(const std::string& str, unsigned size, float x, float y,
                      sf::Color color);

    void drawEntities(sf::RenderWindow& window, const goap::Simulation& sim);
    void drawAgents(sf::RenderWindow& window, const goap::Simulation& sim,
                    bool showLabels, float runTime);
    void drawHud(sf::RenderWindow& window, const goap::Simulation& sim,
                 bool paused, float runTime);
    void drawLog(sf::RenderWindow& window, const goap::Simulation& sim);
    void drawHelp(sf::RenderWindow& window);
};
