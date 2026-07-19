#pragma once
// Action.h
// action = name + cost + preconditions + effects. execution is separate,
// Simulation.cpp switches on the name.

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

// full action list. meleeCheap flips melee 20 -> 1 (T key). rebuilt every
// plan so the toggle applies immediately.
std::vector<Action> buildActionList(bool meleeCheap);

} // namespace goap
