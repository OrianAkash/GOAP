#pragma once
// Renderer.h
// draws the world, agent goal/plan labels, HUD, event log and help overlay.
// read-only view of the simulation, no game logic in here.

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "../contract/Contract.h"

class Renderer {
public:
    // loads the font, false if nothing usable found
    bool init();

    void draw(sf::RenderWindow& window, const goap::Simulation& sim,
              bool paused, bool showLabels, bool showHelp, float runTime);

    // spawn-rate slider. sliderPress returns true if the click landed on it
    // so main.cpp doesn't also spawn a threat from the same click.
    bool sliderPress(float mx, float my);
    void sliderDrag(float mx);          // move whichever slider is being dragged
    void sliderRelease();               // let go of all sliders
    void resetSliders();                // back to defaults (R key)
    int  sliderSpawnLevel() const;      // 0..10 spawn-rate level

private:
    sf::Font m_font;

    // minimal draggable slider, SFML has no widgets
    struct Slider {
        std::string label;
        float x = 0.f, y = 0.f, w = 0.f;   // track geometry, screen pixels
        int minv = 0, maxv = 1, value = 0;
        bool dragging = false;
        bool accent = false;               // draw a red "intensity" fill (spawn dial)
    };
    std::vector<Slider> m_sliders;         // [0] = spawn rate
    void initSliders();
    void drawSliders(sf::RenderWindow& window);

    // one-liner text helper
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
