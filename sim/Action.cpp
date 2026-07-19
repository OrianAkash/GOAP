#include "Action.h"

namespace goap {

// movement actions also set the OTHER location facts false. without this the
// planner made orders like GoToWeapon > ApproachThreat > PickUpWeapon (walks
// away from the gun, then "picks it up"). clearing them makes those orderings
// contradict during the search so they get pruned.

std::vector<Action> buildActionList(bool meleeCheap)
{
    std::vector<Action> list;

    {
        Action a;
        a.name = "GoToWeapon";
        a.cost = 4;
        a.pre.set(Fact::WeaponAvailable, true);
        a.eff.set(Fact::NextToWeapon, true);
        a.eff.set(Fact::AtPatrolPoint, false);
        a.eff.set(Fact::AtOutpost, false);
        a.eff.set(Fact::ThreatInRange, false);
        list.push_back(a);
    }
    {
        Action a;
        a.name = "PickUpWeapon";
        a.cost = 1;
        a.pre.set(Fact::NextToWeapon, true);
        a.pre.set(Fact::WeaponAvailable, true);
        a.eff.set(Fact::HasWeapon, true);
        list.push_back(a);
    }
    {
        Action a;
        a.name = "ApproachThreat";
        a.cost = 3;
        a.pre.set(Fact::ThreatExists, true);
        a.eff.set(Fact::ThreatInRange, true);
        a.eff.set(Fact::AtPatrolPoint, false);
        a.eff.set(Fact::AtOutpost, false);
        a.eff.set(Fact::NextToWeapon, false);
        list.push_back(a);
    }
    {
        Action a;
        a.name = "ShootThreat";
        a.cost = 2;
        a.pre.set(Fact::HasWeapon, true);
        a.pre.set(Fact::ThreatInRange, true);
        a.eff.set(Fact::ThreatExists, false);
        list.push_back(a);
    }
    {
        Action a;
        a.name = "MeleeThreat";
        // fallback normally, T key drops it to 1 for the cost demo
        a.cost = meleeCheap ? 1 : 20;
        a.pre.set(Fact::ThreatInRange, true);
        a.eff.set(Fact::ThreatExists, false);
        list.push_back(a);
    }
    {
        Action a;
        a.name = "GoToPatrolPoint";
        a.cost = 1;
        a.eff.set(Fact::AtPatrolPoint, true);
        a.eff.set(Fact::AtOutpost, false);
        a.eff.set(Fact::NextToWeapon, false);
        a.eff.set(Fact::ThreatInRange, false);
        list.push_back(a);
    }
    {
        Action a;
        a.name = "RetreatToOutpost";
        a.cost = 1;
        a.eff.set(Fact::AtOutpost, true);
        a.eff.set(Fact::AtPatrolPoint, false);
        a.eff.set(Fact::NextToWeapon, false);
        a.eff.set(Fact::ThreatInRange, false);
        list.push_back(a);
    }
    {
        Action a;
        a.name = "HealAtOutpost";
        a.cost = 1;
        a.pre.set(Fact::AtOutpost, true);
        a.eff.set(Fact::LowHealth, false);
        list.push_back(a);
    }
    {
        // radio for backup, only usable at the outpost
        Action a;
        a.name = "CallReinforcements";
        a.cost = 1;
        a.pre.set(Fact::AtOutpost, true);
        a.eff.set(Fact::ReinforcementsCalled, true);
        list.push_back(a);
    }

    return list;
}

} // namespace goap
