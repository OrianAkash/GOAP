# How our GOAP planner works (Part A notes)

These are the notes for the planning half of the demo (`sim/` folder). Most of
this can be reused for the paper and the presentation.

## The idea

In a finite state machine or behaviour tree, someone has to write every
transition by hand: "if you see an enemy and you have no gun, go get a gun,
unless...". GOAP (Orkin, F.E.A.R., 2006) flips this around. We give the agent:

- a **world state**: a handful of true/false facts,
- **goals**: "make this fact have this value" (e.g. `ThreatExists = false`),
- **actions**: each with *preconditions* (facts that must hold to run it) and
  *effects* (facts it changes), plus a *cost*.

Nobody writes behaviours. The agent *searches* for the cheapest sequence of
actions that turns the current state into the goal state. Change the world,
the costs, or the action list, and the behaviour changes on its own. That's
the whole demo.

## Our facts

8 booleans (see `WorldState.h`): `HasWeapon`, `WeaponAvailable`,
`ThreatExists`, `ThreatInRange`, `NextToWeapon`, `AtPatrolPoint`, `AtOutpost`,
`LowHealth`.

The real world (positions, hp, timers) lives in `Simulation.cpp`. Before an
agent plans, we compress the real world into these 8 facts ("snapshot"). The
planner never sees a coordinate - that separation is what makes it fast and
easy to reason about.

## Our actions

| action | pre | eff | cost |
|---|---|---|---|
| GoToWeapon | WeaponAvailable | NextToWeapon | 4 |
| PickUpWeapon | NextToWeapon, WeaponAvailable | HasWeapon | 1 |
| ApproachThreat | ThreatExists | ThreatInRange | 3 |
| ShootThreat | HasWeapon, ThreatInRange | !ThreatExists | 2 |
| MeleeThreat | ThreatInRange | !ThreatExists | 20 (or 1) |
| GoToPatrolPoint | - | AtPatrolPoint | 1 |
| RetreatToOutpost | - | AtOutpost | 1 |
| HealAtOutpost | AtOutpost | !LowHealth | 1 |

(Movement actions additionally set the *other* location facts to false - see
"the ordering bug" below.)

Goals, in priority order: **Survive** (`!LowHealth`, when hp < 35),
**EliminateThreat** (`!ThreatExists`, when threats exist), **Patrol**
(`AtPatrolPoint`, otherwise). Highest applicable one wins, re-checked every
tick.

## Regressive A*

We search **backwards from the goal**, the way Orkin describes it. A search
node is not a world state - it's *the set of conditions we still need to
satisfy* ("open conditions"), plus the actions picked so far.

- Start node: the goal's conditions.
- We're **done** when every open condition is already true in the current
  world snapshot.
- To expand a node, we look for actions whose *effects* satisfy at least one
  open condition (and don't contradict another one). The new node's open set =
  (old open set − what the effects gave us) + the action's *preconditions*.
  The action is prepended, so the finished plan comes out in execution order.
- Costs: `g` = sum of action costs so far, `h` = number of open conditions the
  current world doesn't satisfy yet. `h` is admissible because every action
  costs at least 1 and can close at most... well, at least one condition needs
  at least one action. So A* finds the cheapest plan.
- Safety rails: max 512 node expansions, max plan length 8, and a closed set
  so we don't expand the same open-set twice. An impossible goal just returns
  an empty plan (tested in `planner_tests.cpp`) - the agent shrugs and retries
  half a second later. The demo must never freeze.

### Worked example (the one to show in the presentation)

Agent: unarmed, a weapon crate exists, a threat exists. Goal:
`ThreatExists = false`.

```
open: { ThreatExists=F }
  <- ShootThreat  (eff: ThreatExists=F)         open becomes { HasWeapon, ThreatInRange }
  <- PickUpWeapon (eff: HasWeapon)              open becomes { ThreatInRange, NextToWeapon, WeaponAvailable }
  <- ApproachThreat (eff: ThreatInRange)        open becomes { NextToWeapon, WeaponAvailable, ThreatExists }
       ...wait, this branch dies! ApproachThreat also sets NextToWeapon=F,
       which contradicts the open NextToWeapon=T. rejected.
  <- GoToWeapon  (eff: NextToWeapon)            open becomes { ThreatInRange, WeaponAvailable }
  <- ApproachThreat (eff: ThreatInRange)        open becomes { WeaponAvailable, ThreatExists }
both remaining conditions are true in the current world -> DONE
plan = GoToWeapon > PickUpWeapon > ApproachThreat > ShootThreat   (cost 10)
```

The melee alternative (`ApproachThreat > MeleeThreat`) costs 3+20=23, so A*
prefers fetching the weapon. Delete all weapons mid-fetch and the fetch branch
becomes impossible - the next replan returns the melee plan. Nobody scripted
that fallback; it falls out of the search.

## The ordering bug (and why movement actions clear location facts)

Our first version produced plans like:

```
GoToWeapon > ApproachThreat > PickUpWeapon > ShootThreat
```

Symbolically fine (`NextToWeapon` was made true at some point, and pure
regression doesn't care *when*), physically nonsense: the agent walks to the
weapon, walks away to the threat, then "picks up" a weapon that's 300 pixels
behind it. The fix: every movement action's effects also set the other
location facts to false (walking somewhere means you're no longer at the old
place). Now the silly orderings contradict themselves during the search and
get pruned automatically. This is our budget version of what real planners
solve with full state tracking, and it's a nice example of "your planner is
only as good as your action model".

## Plans are beliefs, not promises

`ShootThreat` claims the effect `ThreatExists = false`, but it really only
kills *one* threat. That's intentional and very GOAP: effects describe what
the agent *believes* the action achieves. Reality wins: after the plan
finishes, goal re-evaluation sees threats still exist and simply plans again.
The same machinery covers everything unexpected:

Replan triggers (see `updateAgent` in `Simulation.cpp`):
1. the goal changed (higher-priority goal became active),
2. the current action's preconditions stopped being true (weapon stolen,
   threats already dead, ...),
3. an action/plan completed,
4. action costs changed (the melee toggle) - everyone replans once.

Plus a 0.25s cooldown between replans so agents can't machine-gun the planner.

## The cost toggle

Pressing T in the demo sets melee cost from 20 to 1. Suddenly
`ApproachThreat > MeleeThreat` (cost 4) is cheaper than
`ApproachThreat > ShootThreat` (cost 5), and every agent - even armed ones -
switches to punching. One number changed, no code changed, completely
different squad behaviour. That's the "action costs shape behaviour" part of
our research question in one keypress.

## Limitations / future work

- Facts are booleans only. Real systems (and F.E.A.R.) allow enums/numbers
  with per-fact distance heuristics.
- Agents plan independently - no squad coordination, they happily dogpile the
  same threat. GOAP squads usually add a coordinator on top.
- Costs are hand-tuned constants; distance-based costs would make plans
  situational (fetch the gun only if it's close).
- The snapshot targets the *nearest* threat/weapon; a target-selection layer
  would be the next step.
