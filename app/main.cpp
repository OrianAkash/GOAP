// main.cpp
// window, input and the main loop. all game logic sits behind the Simulation
// interface, this file just reads input, ticks the sim and draws it.

#include <SFML/Graphics.hpp>
#include "../contract/Contract.h"
#include "Renderer.h"

using namespace goap;

int main()
{
    Simulation sim;

    sf::RenderWindow window(sf::VideoMode(1280, 720),
                            "GOAP Outpost Defense - Research Demo");
    window.setFramerateLimit(60);

    Renderer renderer;
    if (!renderer.init())
        return 1;  // missing font, message already printed

    bool paused = false;
    bool showLabels = true;
    bool showHelp = true;   // start on the instructions overlay
    float runTime = 0.f;
    sf::Clock clock;

    while (window.isOpen()) {
        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Closed) {
                window.close();
                continue;
            }

            // any input closes the help screen, ESC still quits
            if (showHelp) {
                if (e.type == sf::Event::KeyPressed ||
                    e.type == sf::Event::MouseButtonPressed) {
                    if (e.type == sf::Event::KeyPressed &&
                        e.key.code == sf::Keyboard::Escape)
                        window.close();
                    else
                        showHelp = false;
                }
                continue;
            }

            if (e.type == sf::Event::MouseButtonPressed) {
                Vec2 p = { (float)e.mouseButton.x, (float)e.mouseButton.y };
                if (e.mouseButton.button == sf::Mouse::Left) {
                    // slider click shouldn't also spawn a threat
                    if (!renderer.sliderPress(p.x, p.y))
                        sim.spawnThreat(p);
                }
                else if (e.mouseButton.button == sf::Mouse::Right)
                    sim.spawnWeapon(p);
            }
            else if (e.type == sf::Event::MouseButtonReleased) {
                renderer.sliderRelease();
            }
            else if (e.type == sf::Event::MouseMoved) {
                renderer.sliderDrag((float)e.mouseMove.x);
            }
            else if (e.type == sf::Event::KeyPressed) {
                switch (e.key.code) {
                case sf::Keyboard::Escape:
                    window.close();
                    break;
                case sf::Keyboard::Space:
                    paused = !paused;
                    break;
                case sf::Keyboard::R:
                    sim.reset();
                    renderer.resetSliders();
                    runTime = 0.f;
                    break;
                case sf::Keyboard::W:
                    sim.removeAllWeapons();
                    break;
                case sf::Keyboard::T:
                    sim.toggleMeleeCost();
                    break;
                case sf::Keyboard::C:
                    sim.toggleFocusFire();
                    break;
                case sf::Keyboard::H: {
                    // injure the agent nearest the mouse
                    sf::Vector2i m = sf::Mouse::getPosition(window);
                    sim.injureNearestAgent({ (float)m.x, (float)m.y });
                    break;
                }
                case sf::Keyboard::F1:
                    showLabels = !showLabels;
                    break;
                default:
                    break;
                }
            }
        }

        // dial level 0..10 -> 0..3 threats per second
        sim.setThreatSpawnRate(renderer.sliderSpawnLevel() * 0.3f);

        float dt = clock.restart().asSeconds();
        if (paused || showHelp) {
            sim.tick(0.f);   // frozen, views/commands still valid
        } else {
            sim.tick(dt);    // sim clamps dt spikes
            runTime += dt;
        }

        renderer.draw(window, sim, paused, showLabels, showHelp, runTime);
    }

    return 0;
}
