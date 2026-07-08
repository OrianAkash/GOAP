#pragma once
// Action.h
// A GOAP action = name + cost + preconditions + effects.
// The planner never cares HOW an action is carried out, only what it
// needs (pre) and what it changes (eff). The actual execution (moving,
// shooting...) is handled in Simulation.cpp by looking at the name.

#include <string>
#include <vector>
#include "WorldState.h"

namespace goap {

struct Action {
    std::string name;
    int cost = 1;
    WorldState pre;   // facts that must be true before we can run this
    WorldState eff;   // facts that become true/false after running it
};

// Builds the full list of actions our agents know about.
// meleeCheap flips the melee cost (20 -> 1). We rebuild the list every
// time we plan, so toggling it at runtime changes the plans live.
std::vector<Action> buildActionList(bool meleeCheap);

} // namespace goap
