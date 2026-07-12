#pragma once
// Renderer.h
// Draws everything: the world, the agents with their goal + plan labels,
// the HUD, the event log and the help overlay. Part B of the demo.
// The renderer only READS from the simulation - all game logic is in sim/.

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include "../contract/Contract.h"

class Renderer {
public:
    // loads the font. returns false if assets/DejaVuSans.ttf is missing
    bool init();

    void draw(sf::RenderWindow& window, const goap::Simulation& sim,
              bool paused, bool showLabels, bool showHelp, float runTime);

    // --- spawn-rate slider (v1.1 UI). main.cpp drives this from mouse
    // events and pushes the value into the sim every frame. ---
    // returns true if the press landed on the slider, so the caller knows NOT
    // to also treat that click as "spawn a threat".
    bool sliderPress(float mx, float my);
    void sliderDrag(float mx);          // move whichever slider is being dragged
    void sliderRelease();               // let go of all sliders
    void resetSliders();                // back to defaults (R key)
    int  sliderSpawnLevel() const;      // 0..10 spawn-rate level

private:
    sf::Font m_font;

    // a minimal draggable slider (SFML has no widgets, so we roll our own)
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
