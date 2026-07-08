#include "Action.h"

namespace goap {

// IMPORTANT design note (took us a while to figure this out):
// every movement action also sets the OTHER location facts to false,
// because walking somewhere means you are no longer at the old place.
// Without this the planner can output plans that are logically fine but
// physically nonsense, e.g.
//     GoToWeapon > ApproachThreat > PickUpWeapon
// (walk to the weapon, walk away to the threat, then "pick up" a weapon
// that is nowhere near you). With the location facts cleared, that
// ordering contradicts itself during the search and gets rejected, so
// only the sensible order survives. See docs/PLANNER.md.

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
        // normally a desperate fallback (cost 20), but the demo can make it
        // cost 1 to show how cost tuning changes the agents' behaviour
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

    return list;
}

} // namespace goap
