// planner_tests.cpp
// planner sanity tests, no framework. build with build_tests.bat.

#include "../sim/Planner.h"
#include <iostream>
#include <string>

using namespace goap;

static int failures = 0;

// full state in one call
static WorldState makeState(bool hasWeapon, bool weaponAvail, bool threatExists,
                            bool threatInRange, bool nextToWeapon,
                            bool atPatrol, bool atOutpost, bool lowHealth)
{
    WorldState s;
    s.set(Fact::HasWeapon, hasWeapon);
    s.set(Fact::WeaponAvailable, weaponAvail);
    s.set(Fact::ThreatExists, threatExists);
    s.set(Fact::ThreatInRange, threatInRange);
    s.set(Fact::NextToWeapon, nextToWeapon);
    s.set(Fact::AtPatrolPoint, atPatrol);
    s.set(Fact::AtOutpost, atOutpost);
    s.set(Fact::LowHealth, lowHealth);
    return s;
}

static std::string planToString(const std::vector<Action>& plan)
{
    if (plan.empty())
        return "(empty)";
    std::string out;
    for (std::size_t i = 0; i < plan.size(); i++) {
        if (i > 0)
            out += " > ";
        out += plan[i].name;
    }
    return out;
}

static void check(const std::string& name, const std::vector<Action>& plan,
                  const std::string& expected)
{
    std::string got = planToString(plan);
    if (got == expected) {
        std::cout << "PASS  " << name << "\n      " << got << "\n";
    } else {
        std::cout << "FAIL  " << name << "\n      expected: " << expected
                  << "\n      got:      " << got << "\n";
        failures++;
    }
}

// apply the plan forwards and check every precondition holds
static void checkForwardValid(const std::string& name, WorldState state,
                              const std::vector<Action>& plan)
{
    for (auto& action : plan) {
        if (!state.satisfies(action.pre)) {
            std::cout << "FAIL  " << name << " - precondition of "
                      << action.name << " does not hold\n";
            failures++;
            return;
        }
        for (auto& kv : action.eff.facts)
            state.set(kv.first, kv.second);
    }
    std::cout << "PASS  " << name << "\n";
}

int main()
{
    std::vector<Action> actions = buildActionList(false);
    std::vector<Action> actionsCheap = buildActionList(true);

    WorldState killGoal;
    killGoal.set(Fact::ThreatExists, false);
    WorldState surviveGoal;
    surviveGoal.set(Fact::LowHealth, false);
    WorldState patrolGoal;
    patrolGoal.set(Fact::AtPatrolPoint, true);

    // 1. armed and a threat shows up -> approach and shoot
    check("armed vs threat",
          makePlan(makeState(true, true, true, false, false, false, false, false),
                   killGoal, actions),
          "ApproachThreat > ShootThreat");

    // 2. unarmed but a weapon exists -> the big 4 step plan.
    //    fetching the weapon (4+1+3+2=10) beats melee (3+20=23)
    check("unarmed, weapon available",
          makePlan(makeState(false, true, true, false, false, false, false, false),
                   killGoal, actions),
          "GoToWeapon > PickUpWeapon > ApproachThreat > ShootThreat");

    // 3. unarmed and NO weapons anywhere -> melee is the only option
    check("unarmed, no weapons in world",
          makePlan(makeState(false, false, true, false, false, false, false, false),
                   killGoal, actions),
          "ApproachThreat > MeleeThreat");

    // 4. melee toggled cheap -> even an armed agent prefers melee (4 < 5)
    check("armed but melee is cheap",
          makePlan(makeState(true, true, true, false, false, false, false, false),
                   killGoal, actionsCheap),
          "ApproachThreat > MeleeThreat");

    // 5. low health -> run home and heal
    check("low health",
          makePlan(makeState(false, true, true, false, false, false, false, true),
                   surviveGoal, actions),
          "RetreatToOutpost > HealAtOutpost");

    // 6. nothing going on -> patrol
    check("peace time patrol",
          makePlan(makeState(false, true, false, false, false, false, false, false),
                   patrolGoal, actions),
          "GoToPatrolPoint");

    // 7. impossible goal must return empty and NOT hang.
    //    goal: HasWeapon, but no weapon exists and nothing can create one
    {
        WorldState impossibleGoal;
        impossibleGoal.set(Fact::HasWeapon, true);
        check("impossible goal gives up cleanly",
              makePlan(makeState(false, false, false, false, false, false, false, false),
                       impossibleGoal, actions),
              "(empty)");
    }

    // 8. goal already satisfied -> empty plan (nothing to do)
    check("goal already satisfied",
          makePlan(makeState(false, true, false, false, false, true, false, false),
                   patrolGoal, actions),
          "(empty)");

    // 9. the 4 step plan must also be valid when simulated forwards
    checkForwardValid("4 step plan is forward-valid",
                      makeState(false, true, true, false, false, false, false, false),
                      makePlan(makeState(false, true, true, false, false, false, false, false),
                               killGoal, actions));

    std::cout << "\n" << (failures == 0 ? "all planner tests passed"
                                        : "SOME TESTS FAILED") << "\n";
    return failures == 0 ? 0 : 1;
}
