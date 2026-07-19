#pragma once
// WorldState.h
// symbolic world state for the planner. the real world (positions, hp,
// timers) lives in Simulation.cpp and gets snapshotted into these facts.

#include <map>
#include <string>

namespace goap {

enum class Fact {
    HasWeapon,
    WeaponAvailable,   // there is a weapon pickup somewhere in the world
    ThreatExists,
    ThreatInRange,     // nearest threat is inside our attack range
    NextToWeapon,      // close enough to pick a weapon up
    AtPatrolPoint,
    AtOutpost,
    LowHealth,
    ReinforcementsCalled   // squad has radioed for backup this wave
};

const char* factName(Fact f);

// fact -> bool map. used for the current state, action pre/eff and goals.
struct WorldState {
    std::map<Fact, bool> facts;

    void set(Fact f, bool value);
    bool has(Fact f) const;   // present at all?
    bool get(Fact f) const;   // value, false if missing

    // every fact in 'conditions' matches us
    bool satisfies(const WorldState& conditions) const;

    // mismatch count, used as the A* heuristic
    int countUnsatisfied(const WorldState& conditions) const;

    // string key for the closed set
    std::string encode() const;
};

} // namespace goap
