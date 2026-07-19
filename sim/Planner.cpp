#include "Planner.h"

#include <queue>
#include <set>

namespace goap {

namespace {

// search node: conditions still needed + actions picked so far
struct Node {
    WorldState open;            // conditions still unsatisfied
    std::vector<Action> plan;   // actions picked so far, in execution order
    int g = 0;                  // total cost of those actions
    int f = 0;                  // g + heuristic
    int seq = 0;                // tie breaker -> deterministic results
};

struct NodeCompare {
    bool operator()(const Node& a, const Node& b) const {
        if (a.f != b.f)
            return a.f > b.f;  // lower f gets popped first
        return a.seq > b.seq;  // same f -> first pushed, first popped
    }
};

} // namespace

std::vector<Action> makePlan(const WorldState& current,
                             const WorldState& goal,
                             const std::vector<Action>& actions)
{
    // caps so a bad goal can't freeze anything
    const int MAX_EXPANSIONS = 512;
    const int MAX_PLAN_LENGTH = 8;

    if (current.satisfies(goal))
        return {};  // nothing to do

    std::priority_queue<Node, std::vector<Node>, NodeCompare> openList;
    std::set<std::string> closed;

    Node start;
    start.open = goal;
    start.g = 0;
    start.f = current.countUnsatisfied(goal);
    start.seq = 0;
    openList.push(start);

    int seqCounter = 1;
    int expansions = 0;

    while (!openList.empty() && expansions < MAX_EXPANSIONS) {
        Node node = openList.top();
        openList.pop();

        // done when the world already satisfies everything left
        if (current.satisfies(node.open))
            return node.plan;

        // already expanded?
        std::string key = node.open.encode();
        if (closed.count(key))
            continue;
        closed.insert(key);
        expansions++;

        if ((int)node.plan.size() >= MAX_PLAN_LENGTH)
            continue;

        for (const Action& action : actions) {
            // usable if an effect satisfies an open condition and none break one
            bool useful = false;
            bool contradicts = false;
            for (auto& kv : action.eff.facts) {
                if (node.open.has(kv.first)) {
                    if (node.open.get(kv.first) == kv.second)
                        useful = true;
                    else
                        contradicts = true;
                }
            }
            if (!useful || contradicts)
                continue;

            // regress: open = (open - satisfied by effects) + preconditions
            Node child;
            child.open = node.open;
            for (auto& kv : action.eff.facts) {
                if (child.open.has(kv.first) && child.open.get(kv.first) == kv.second)
                    child.open.facts.erase(kv.first);
            }
            bool impossible = false;
            for (auto& kv : action.pre.facts) {
                if (child.open.has(kv.first) && child.open.get(kv.first) != kv.second) {
                    // fact needed both true and false -> dead branch
                    impossible = true;
                    break;
                }
                child.open.set(kv.first, kv.second);
            }
            if (impossible)
                continue;

            // prepend so the plan comes out in execution order
            child.plan = node.plan;
            child.plan.insert(child.plan.begin(), action);
            child.g = node.g + action.cost;
            child.f = child.g + current.countUnsatisfied(child.open);
            child.seq = seqCounter++;
            openList.push(child);
        }
    }

    return {};  // no plan
}

} // namespace goap
