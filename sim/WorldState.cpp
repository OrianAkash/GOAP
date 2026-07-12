#include "WorldState.h"

namespace goap {

const char* factName(Fact f)
{
    switch (f) {
    case Fact::HasWeapon:       return "HasWeapon";
    case Fact::WeaponAvailable: return "WeaponAvailable";
    case Fact::ThreatExists:    return "ThreatExists";
    case Fact::ThreatInRange:   return "ThreatInRange";
    case Fact::NextToWeapon:    return "NextToWeapon";
    case Fact::AtPatrolPoint:   return "AtPatrolPoint";
    case Fact::AtOutpost:       return "AtOutpost";
    case Fact::LowHealth:       return "LowHealth";
    case Fact::ReinforcementsCalled: return "ReinforcementsCalled";
    }
    return "???";
}

void WorldState::set(Fact f, bool value)
{
    facts[f] = value;
}

bool WorldState::has(Fact f) const
{
    return facts.find(f) != facts.end();
}

bool WorldState::get(Fact f) const
{
    auto it = facts.find(f);
    if (it == facts.end())
        return false;
    return it->second;
}

bool WorldState::satisfies(const WorldState& conditions) const
{
    for (auto& kv : conditions.facts) {
        auto it = facts.find(kv.first);
        if (it == facts.end() || it->second != kv.second)
            return false;
    }
    return true;
}

int WorldState::countUnsatisfied(const WorldState& conditions) const
{
    int count = 0;
    for (auto& kv : conditions.facts) {
        auto it = facts.find(kv.first);
        if (it == facts.end() || it->second != kv.second)
            count++;
    }
    return count;
}

std::string WorldState::encode() const
{
    // something like "0=1|3=0|" - just needs to be unique per state
    std::string out;
    for (auto& kv : facts) {
        out += std::to_string((int)kv.first);
        out += kv.second ? "=1|" : "=0|";
    }
    return out;
}

} // namespace goap
