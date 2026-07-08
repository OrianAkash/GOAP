#pragma once
// Planner.h
// The GOAP planner. Regressive A*: we search BACKWARDS from the goal
// towards the current state (this is how Orkin describes it for F.E.A.R.).
//
// Returns the plan in execution order (plan[0] runs first).
// An empty vector means either "no plan exists" or "the goal is already
// satisfied" - the caller (Simulation) checks goal satisfaction itself
// before calling us, so it can tell the two apart.

#include <vector>
#include "Action.h"

namespace goap {

std::vector<Action> makePlan(const WorldState& current,
                             const WorldState& goal,
                             const std::vector<Action>& actions);

} // namespace goap
