#pragma once
// Planner.h
// regressive A*: search backwards from the goal (Orkin style).
// returns the plan in execution order. empty = no plan / already satisfied,
// Simulation checks satisfaction itself so it can tell the two apart.

#include <vector>
#include "Action.h"

namespace goap {

std::vector<Action> makePlan(const WorldState& current,
                             const WorldState& goal,
                             const std::vector<Action>& actions);

} // namespace goap
