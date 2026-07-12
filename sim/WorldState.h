#pragma once
// WorldState.h
// The symbolic world state the GOAP planner works with.
// Everything the planner knows about the world is boiled down to these
// 8 boolean facts. The actual game world (positions, hp, timers...) lives
// in Simulation.cpp - before planning we take a "snapshot" of it into
// one of these.

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

// A set of fact -> bool pairs. Used for 3 different things:
//  - the agent's current state (all 8 facts filled in)
//  - action preconditions / effects (only the facts that matter)
//  - goal states (usually just 1 fact)
struct WorldState {
    std::map<Fact, bool> facts;

    void set(Fact f, bool value);
    bool has(Fact f) const;   // is this fact present in the map at all
    bool get(Fact f) const;   // its value (false if not present)

    // true if every fact in 'conditions' is present in us with the same value.
    // e.g. currentState.satisfies(goalState) or currentState.satisfies(action.pre)
    bool satisfies(const WorldState& conditions) const;

    // how many facts in 'conditions' don't match us. used as the A* heuristic.
    int countUnsatisfied(const WorldState& conditions) const;

    // turns the state into a string key so the planner can keep a closed set
    std::string encode() const;
};

} // namespace goap
