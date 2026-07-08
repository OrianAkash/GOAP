// console_main.cpp
// Headless smoke test: runs the full simulation for 60 sim-seconds with some
// scripted events (same things the demo hotkeys will do) and prints what the
// agents are up to. Lets us test Part A before the SFML app (Part B) exists.

#include "../contract/Contract.h"
#include <cstdio>

using namespace goap;

static const char* phaseName(AgentPhase p)
{
    switch (p) {
    case AgentPhase::Idle:     return "Idle";
    case AgentPhase::Planning: return "Planning";
    case AgentPhase::Moving:   return "Moving";
    case AgentPhase::Acting:   return "Acting";
    case AgentPhase::Dead:     return "Dead";
    }
    return "?";
}

static void printStatus(const Simulation& sim, float t)
{
    std::printf("---- t=%.0fs ----\n", t);
    for (const AgentView& a : sim.agents()) {
        std::printf("  A%d hp=%3.0f %-8s goal=%-16s ", a.id, a.health,
                    phaseName(a.phase), a.goalName.c_str());
        if (a.plan.empty()) {
            std::printf("(no plan)\n");
        } else {
            for (std::size_t i = 0; i < a.plan.size(); i++) {
                if (i == a.currentStep)
                    std::printf("[%s] ", a.plan[i].c_str());
                else
                    std::printf("%s ", a.plan[i].c_str());
            }
            std::printf("\n");
        }
    }
}

int main()
{
    Simulation sim;
    const float dt = 1.0f / 60.0f;

    bool did5 = false, did7 = false, did20 = false, did30 = false, did45 = false, did46 = false;
    float nextStatus = 0.f;

    for (float t = 0.f; t < 60.f; t += dt) {
        // scripted events, same as the demo hotkeys:
        if (!did5 && t >= 5.f) {
            did5 = true;
            sim.spawnThreat({1200.f, 100.f});   // unarmed agents must fetch weapons
            sim.spawnThreat({80.f, 600.f});
        }
        if (!did7 && t >= 6.5f) {
            did7 = true;
            sim.removeAllWeapons();             // agents are mid-fetch -> REPLAN
        }
        if (!did20 && t >= 20.f) {
            did20 = true;
            sim.spawnWeapon({500.f, 250.f});    // give them a gun again
            sim.spawnThreat({1200.f, 620.f});
        }
        if (!did30 && t >= 30.f) {
            did30 = true;
            sim.injureNearestAgent({640.f, 360.f});  // Survive goal
        }
        if (!did45 && t >= 45.f) {
            did45 = true;
            sim.toggleMeleeCost();              // cost tuning changes plans
        }
        if (!did46 && t >= 46.f) {
            did46 = true;
            sim.spawnThreat({300.f, 80.f});
        }

        sim.tick(dt);

        if (t >= nextStatus) {
            printStatus(sim, t);
            nextStatus += 10.f;
        }
    }

    std::printf("\n==== event log (last %d entries) ====\n", (int)sim.eventLog().size());
    for (const std::string& line : sim.eventLog())
        std::printf("%s\n", line.c_str());

    // basic sanity so the exit code means something
    if (sim.agents().size() != 3) {
        std::printf("ERROR: expected 3 agents\n");
        return 1;
    }
    bool hasOutpost = false;
    for (const EntityView& e : sim.entities())
        if (e.type == EntityType::Outpost)
            hasOutpost = true;
    if (!hasOutpost) {
        std::printf("ERROR: outpost missing from entity views\n");
        return 1;
    }

    std::printf("\nconsole run finished OK (60 sim-seconds, no crash)\n");
    return 0;
}
