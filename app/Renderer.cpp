#include "Renderer.h"
#include <cmath>
#include <cstdio>

using namespace goap;

// colours in one place
namespace {
const sf::Color COL_BG(25, 28, 34);
const sf::Color COL_OUTPOST(40, 120, 70);
const sf::Color COL_OUTPOST_EDGE(90, 200, 120);
const sf::Color COL_HEALZONE(60, 140, 90, 70);
const sf::Color COL_PATROL(110, 110, 115);
const sf::Color COL_WEAPON(230, 200, 60);
const sf::Color COL_THREAT(205, 70, 60);
const sf::Color COL_AGENT(80, 190, 220);
const sf::Color COL_DEAD(90, 90, 95);
const sf::Color COL_TEXT(235, 235, 235);
const sf::Color COL_DIM(150, 150, 155);
const sf::Color COL_WARN(240, 160, 70);
const sf::Color COL_LINE(255, 255, 255, 60);

const float HEAL_ZONE_RADIUS = 100.f;  // matches HEAL_ZONE in Simulation.cpp
}

void Renderer::initSliders()
{
    m_sliders.clear();

    Slider spawn;
    spawn.label = "SPAWN RATE";
    spawn.x = 1055.f; spawn.y = 58.f; spawn.w = 165.f;
    spawn.minv = 0; spawn.maxv = 10; spawn.value = 0;  // starts calm
    spawn.accent = true;
    m_sliders.push_back(spawn);
}

bool Renderer::init()
{
    initSliders();

    // try a few paths so it works from VS (working dir = project folder)
    // and also when you run the exe straight from x64\Release.
    // last resort: fonts that ship with Windows, so a missing assets folder
    // can never kill the demo.
    const char* paths[] = { "assets/DejaVuSans.ttf",
                            "../assets/DejaVuSans.ttf",
                            "../../assets/DejaVuSans.ttf",
                            "C:/Windows/Fonts/arial.ttf",
                            "C:/Windows/Fonts/segoeui.ttf" };
    for (const char* p : paths) {
        if (m_font.loadFromFile(p))
            return true;
    }
    std::printf("ERROR: could not load any font.\n"
                "       put a .ttf at assets/DejaVuSans.ttf next to the exe\n"
                "       (or next to GOAP.sln when running from Visual Studio).\n");
    return false;
}

sf::Text Renderer::makeText(const std::string& str, unsigned size, float x,
                            float y, sf::Color color)
{
    sf::Text t;
    t.setFont(m_font);
    t.setString(str);
    t.setCharacterSize(size);
    t.setFillColor(color);
    t.setOutlineColor(sf::Color(0, 0, 0, 200));
    t.setOutlineThickness(1.f);
    t.setPosition(x, y);
    return t;
}

void Renderer::draw(sf::RenderWindow& window, const Simulation& sim,
                    bool paused, bool showLabels, bool showHelp, float runTime)
{
    window.clear(COL_BG);
    drawEntities(window, sim);
    drawAgents(window, sim, showLabels, runTime);
    drawHud(window, sim, paused, runTime);
    drawLog(window, sim);
    drawSliders(window);
    if (showHelp)
        drawHelp(window);
    window.display();
}

// ---- spawn-rate slider -----------------------------------------------------

void Renderer::drawSliders(sf::RenderWindow& window)
{
    // grouping panel so the control reads as a unit, not a floating bit
    sf::RectangleShape panel(sf::Vector2f(272.f, 58.f));
    panel.setPosition(1000.f, 26.f);
    panel.setFillColor(sf::Color(0, 0, 0, 110));
    panel.setOutlineColor(sf::Color(255, 255, 255, 30));
    panel.setOutlineThickness(1.f);
    window.draw(panel);

    for (const Slider& s : m_sliders) {
        window.draw(makeText(s.label, 12, s.x, s.y - 18.f,
                             s.accent ? COL_THREAT : COL_DIM));

        float frac = (float)(s.value - s.minv) / (float)(s.maxv - s.minv);
        float hx = s.x + frac * s.w;

        // empty track
        sf::RectangleShape track(sf::Vector2f(s.w, 3.f));
        track.setPosition(s.x, s.y - 1.5f);
        track.setFillColor(sf::Color(120, 120, 128));
        window.draw(track);

        // for the pressure dial, fill the used portion red so it reads as a
        // danger/intensity gauge at a glance
        if (s.accent && frac > 0.f) {
            sf::RectangleShape fill(sf::Vector2f(frac * s.w, 3.f));
            fill.setPosition(s.x, s.y - 1.5f);
            fill.setFillColor(COL_THREAT);
            window.draw(fill);
        }

        sf::CircleShape handle(7.f);
        handle.setOrigin(7.f, 7.f);
        handle.setPosition(hx, s.y);
        sf::Color handleCol = s.accent ? COL_THREAT : sf::Color(210, 210, 215);
        if (s.dragging) handleCol = COL_WEAPON;
        handle.setFillColor(handleCol);
        window.draw(handle);

        char val[16];
        std::snprintf(val, sizeof(val), "%d", s.value);
        window.draw(makeText(val, 13, s.x + s.w + 12.f, s.y - 9.f, COL_TEXT));
    }
}

bool Renderer::sliderPress(float mx, float my)
{
    for (Slider& s : m_sliders) {
        float frac = (float)(s.value - s.minv) / (float)(s.maxv - s.minv);
        float hx = s.x + frac * s.w;
        bool onTrack = (mx >= s.x - 12.f && mx <= s.x + s.w + 12.f &&
                        my >= s.y - 11.f && my <= s.y + 11.f);
        float dx = mx - hx, dy = my - s.y;
        bool onHandle = (dx * dx + dy * dy) <= 13.f * 13.f;
        if (onTrack || onHandle) {
            s.dragging = true;
            sliderDrag(mx);   // jump the handle to the click position
            return true;
        }
    }
    return false;
}

void Renderer::sliderDrag(float mx)
{
    for (Slider& s : m_sliders) {
        if (!s.dragging)
            continue;
        float clamped = mx;
        if (clamped < s.x) clamped = s.x;
        if (clamped > s.x + s.w) clamped = s.x + s.w;
        float frac = (clamped - s.x) / s.w;
        s.value = s.minv + (int)std::lround(frac * (s.maxv - s.minv));
    }
}

void Renderer::sliderRelease()
{
    for (Slider& s : m_sliders)
        s.dragging = false;
}

void Renderer::resetSliders()
{
    initSliders();
}

int Renderer::sliderSpawnLevel() const
{
    return m_sliders.empty() ? 0 : m_sliders[0].value;
}

void Renderer::drawEntities(sf::RenderWindow& window, const Simulation& sim)
{
    for (const EntityView& e : sim.entities()) {
        if (e.type == EntityType::Outpost) {
            // heal zone ring first, then the building
            sf::CircleShape zone(HEAL_ZONE_RADIUS);
            zone.setOrigin(HEAL_ZONE_RADIUS, HEAL_ZONE_RADIUS);
            zone.setPosition(e.pos.x, e.pos.y);
            zone.setFillColor(sf::Color::Transparent);
            zone.setOutlineColor(COL_HEALZONE);
            zone.setOutlineThickness(2.f);
            window.draw(zone);

            sf::RectangleShape box(sf::Vector2f(80.f, 80.f));
            box.setOrigin(40.f, 40.f);
            box.setPosition(e.pos.x, e.pos.y);
            box.setFillColor(COL_OUTPOST);
            box.setOutlineColor(COL_OUTPOST_EDGE);
            box.setOutlineThickness(2.f);
            window.draw(box);

            // outpost hp bar
            float frac = e.maxHealth > 0.f ? e.health / e.maxHealth : 0.f;
            sf::RectangleShape barBg(sf::Vector2f(80.f, 6.f));
            barBg.setPosition(e.pos.x - 40.f, e.pos.y - 54.f);
            barBg.setFillColor(sf::Color(0, 0, 0, 160));
            window.draw(barBg);
            sf::RectangleShape bar(sf::Vector2f(80.f * frac, 6.f));
            bar.setPosition(e.pos.x - 40.f, e.pos.y - 54.f);
            bar.setFillColor(COL_OUTPOST_EDGE);
            window.draw(bar);
        }
        else if (e.type == EntityType::PatrolPoint) {
            sf::CircleShape c(6.f);
            c.setOrigin(6.f, 6.f);
            c.setPosition(e.pos.x, e.pos.y);
            c.setFillColor(sf::Color::Transparent);
            c.setOutlineColor(COL_PATROL);
            c.setOutlineThickness(2.f);
            window.draw(c);
        }
        else if (e.type == EntityType::Weapon) {
            sf::CircleShape tri(10.f, 3);  // a triangle
            tri.setOrigin(10.f, 10.f);
            tri.setPosition(e.pos.x, e.pos.y);
            tri.setFillColor(COL_WEAPON);
            window.draw(tri);
        }
        else if (e.type == EntityType::Threat) {
            sf::CircleShape c(9.f);
            c.setOrigin(9.f, 9.f);
            c.setPosition(e.pos.x, e.pos.y);
            c.setFillColor(COL_THREAT);
            window.draw(c);

            float frac = e.maxHealth > 0.f ? e.health / e.maxHealth : 0.f;
            sf::RectangleShape bar(sf::Vector2f(24.f * frac, 3.f));
            bar.setPosition(e.pos.x - 12.f, e.pos.y - 17.f);
            bar.setFillColor(COL_THREAT);
            window.draw(bar);
        }
    }
}

void Renderer::drawAgents(sf::RenderWindow& window, const Simulation& sim,
                          bool showLabels, float runTime)
{
    for (const AgentView& a : sim.agents()) {
        bool dead = (a.phase == AgentPhase::Dead);

        // line to whatever the agent is moving to / attacking
        if (!dead && a.hasTarget) {
            sf::Vertex line[2] = {
                sf::Vertex(sf::Vector2f(a.pos.x, a.pos.y), COL_LINE),
                sf::Vertex(sf::Vector2f(a.targetPos.x, a.targetPos.y), COL_LINE)
            };
            window.draw(line, 2, sf::Lines);
        }

        sf::CircleShape body(10.f);
        body.setOrigin(10.f, 10.f);
        body.setPosition(a.pos.x, a.pos.y);
        if (dead) {
            body.setFillColor(sf::Color::Transparent);
            body.setOutlineColor(COL_DEAD);
            body.setOutlineThickness(2.f);
        } else {
            sf::Color col = COL_AGENT;
            // blink red-ish when hurt (matches the Survive threshold)
            if (a.health < 35.f && std::fmod(runTime, 0.6f) < 0.3f)
                col = sf::Color(230, 90, 80);
            body.setFillColor(col);
        }
        window.draw(body);

        // little yellow dot = carrying a weapon
        if (!dead && a.hasWeapon) {
            sf::CircleShape dot(3.5f);
            dot.setOrigin(3.5f, 3.5f);
            dot.setPosition(a.pos.x + 7.f, a.pos.y - 7.f);
            dot.setFillColor(COL_WEAPON);
            window.draw(dot);
        }

        if (!showLabels)
            continue;

        // line 1: "A1 [EliminateThreat] 87hp"
        char line1[64];
        std::snprintf(line1, sizeof(line1), "A%d [%s] %.0fhp", a.id,
                      a.goalName.c_str(), a.health);
        window.draw(makeText(line1, 13, a.pos.x - 40.f, a.pos.y - 44.f,
                             dead ? COL_DIM : COL_TEXT));

        // line 2: the plan, current action in [brackets]
        std::string line2;
        if (dead) {
            line2 = "(dead)";
        } else if (a.plan.empty()) {
            line2 = (a.phase == AgentPhase::Planning) ? "(planning...)" : "(idle)";
        } else {
            for (std::size_t i = 0; i < a.plan.size(); i++) {
                if (i > 0)
                    line2 += " > ";
                if (i == a.currentStep)
                    line2 += "[" + a.plan[i] + "]";
                else
                    line2 += a.plan[i];
            }
        }
        window.draw(makeText(line2, 12, a.pos.x - 40.f, a.pos.y - 28.f, COL_DIM));
    }
}

void Renderer::drawHud(sf::RenderWindow& window, const Simulation& sim,
                       bool paused, float runTime)
{
    // controls, always visible top-left
    window.draw(makeText(
        "LMB spawn threat    RMB spawn weapon    W remove weapons\n"
        "H injure agent    T melee cost    C coordination    SPACE pause    R reset    F1 labels    ESC quit",
        13, 10.f, 8.f, COL_DIM));

    // squad coordination state, top-left under the controls
    bool coord = sim.focusFire();
    window.draw(makeText(
        coord ? "squad coordination: ON (defenders split targets)"
              : "squad coordination: OFF (defenders dogpile the nearest)",
        13, 10.f, 44.f, coord ? COL_OUTPOST_EDGE : COL_WARN));

    // "holding the line" banner, centred, when the squad is outnumbered
    if (sim.holdingLine()) {
        sf::Text banner = makeText("OUTNUMBERED - HOLDING THE LINE", 18, 0.f, 40.f,
                                   COL_THREAT);
        banner.setPosition(640.f - banner.getLocalBounds().width * 0.5f, 40.f);
        window.draw(banner);
    }

    // status, top-right
    char status[96];
    std::snprintf(status, sizeof(status), "t=%.1fs%s",
                  runTime, paused ? "   [PAUSED]" : "");
    sf::Text st = makeText(status, 14, 0.f, 8.f,
                           paused ? sf::Color::Yellow : COL_TEXT);
    st.setPosition(1270.f - st.getLocalBounds().width, 8.f);
    window.draw(st);
}

void Renderer::drawLog(sf::RenderWindow& window, const Simulation& sim)
{
    // last 8 event log lines, bottom left. this is the "watch the AI think" bit
    const std::vector<std::string>& log = sim.eventLog();
    std::size_t count = log.size() < 8 ? log.size() : 8;
    float y = 720.f - 18.f * count - 8.f;
    for (std::size_t i = log.size() - count; i < log.size(); i++) {
        bool warn = log[i].find("REPLAN") != std::string::npos ||
                    log[i].find("OVERRUN") != std::string::npos;
        window.draw(makeText(log[i], 13, 10.f, y, warn ? COL_WARN : COL_DIM));
        y += 18.f;
    }
}

void Renderer::drawHelp(sf::RenderWindow& window)
{
    sf::RectangleShape shade(sf::Vector2f(1280.f, 720.f));
    shade.setFillColor(sf::Color(0, 0, 0, 190));
    window.draw(shade);

    float x = 300.f, y = 120.f;
    window.draw(makeText("GOAP Outpost Defense", 32, x, y, COL_TEXT));
    y += 62.f;

    // what it is - short
    window.draw(makeText(
        "3 agents defend the base. Nothing is scripted: each one picks a goal\n"
        "and searches for the cheapest plan to reach it. Change the world and\n"
        "they replan. The label above each agent is its goal and current plan.",
        16, x, y, COL_TEXT));
    y += 92.f;

    // legend - short
    window.draw(makeText(
        "cyan = defender     red = attacker     yellow triangle = weapon\n"
        "green square = base (heal zone)     gray = patrol points",
        16, x, y, COL_DIM));
    y += 70.f;

    // controls - short
    window.draw(makeText(
        "Left click: spawn attacker        Right click: drop weapon\n"
        "Drag SPAWN RATE (top-right): raise the pressure\n"
        "W clear weapons   H injure   T melee cost   C coordination\n"
        "SPACE pause     R reset     F1 labels     ESC quit",
        16, x, y, COL_TEXT));
    y += 108.f;

    window.draw(makeText("Press any key to start", 18, x, y, COL_WARN));
}
