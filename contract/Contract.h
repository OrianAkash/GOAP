#pragma once
// contract/Contract.h — FROZEN INTERFACE (v1.1)
// Do NOT modify this file. Both halves of the project compile against this
// exact file. Any change must be agreed by both teammates and versioned.
//
// v1.1: added setThreatSpawnRate/threatSpawnRate so the UI can auto-spawn
//       threats over time (the SPAWN RATE dial). The whole v1.0 API is
//       unchanged, so existing code keeps working.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace goap {

struct Vec2 { float x = 0.f; float y = 0.f; };

enum class EntityType : std::uint8_t { Outpost, PatrolPoint, Weapon, Threat };

struct EntityView {
    int id = 0;
    EntityType type = EntityType::Outpost;
    Vec2 pos;
    float health = 0.f;      // Outpost/Threat: current HP. Weapon/PatrolPoint: 0.
    float maxHealth = 0.f;   // Outpost: 500, Threat: 75. Others: 0.
};

enum class AgentPhase : std::uint8_t { Idle, Planning, Moving, Acting, Dead };

struct AgentView {
    int id = 0;                        // 1-based (A1, A2, A3)
    Vec2 pos;
    float health = 0.f;                // 0..100
    bool hasWeapon = false;
    AgentPhase phase = AgentPhase::Idle;
    std::string goalName;              // "Patrol" | "EliminateThreat" | "Survive" | "None"
    std::vector<std::string> plan;     // action names; index 0 = first action of the plan
    std::size_t currentStep = 0;       // index into plan of the action being executed
    bool hasTarget = false;
    Vec2 targetPos;                    // current movement/attack target; valid when hasTarget
};

class Simulation {
public:
    Simulation();                // builds the default scenario
    ~Simulation();
    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    void tick(float dtSeconds);  // advance the sim; pass 0.f while paused
                                 // (views and commands remain valid while paused)
    void reset();                // restore the default scenario

    // Commands (UI -> sim). Take effect immediately and are written to eventLog.
    void spawnThreat(Vec2 pos);
    void spawnWeapon(Vec2 pos);
    void removeAllWeapons();           // removes ground pickups only, not carried weapons
    void injureNearestAgent(Vec2 pos); // drops the nearest living agent to 20 HP
    void toggleMeleeCost();            // flips MeleeThreat action cost 20 <-> 1
    void setThreatSpawnRate(float perSecond); // v1.1: auto-spawn threats over time (0 = off)
    void toggleFocusFire();            // v1.1: squad target-splitting on/off (dogpile vs spread)

    // Queries (renderer, every frame). References are valid until the next
    // tick()/command/reset() call.
    const std::vector<AgentView>& agents() const;
    const std::vector<EntityView>& entities() const;
    const std::vector<std::string>& eventLog() const; // newest last, capped at 50
    bool meleeCheap() const;           // current state of the melee-cost toggle
    float threatSpawnRate() const;     // v1.1: current auto-spawn rate (threats/sec)
    bool focusFire() const;            // v1.1: is squad target-splitting on?
    bool holdingLine() const;          // v1.1: is the squad currently outnumbered/defending?
    Vec2 worldSize() const;            // always {1280, 720}

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace goap
