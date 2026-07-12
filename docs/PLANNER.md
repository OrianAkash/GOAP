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

9 booleans (see `WorldState.h`): `HasWeapon`, `WeaponAvailable`,
`ThreatExists`, `ThreatInRange`, `NextToWeapon`, `AtPatrolPoint`, `AtOutpost`,
`LowHealth`, `ReinforcementsCalled`.

The real world (positions, hp, timers) lives in `Simulation.cpp`. Before an
agent plans, we compress the real world into these facts ("snapshot"). The
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
| CallReinforcements | AtOutpost | ReinforcementsCalled | 1 |

(Movement actions additionally set the *other* location facts to false - see
"the ordering bug" below.)

Goals, in priority order: **Survive** (`!LowHealth`, when hp < 35),
**CallForHelp** (`ReinforcementsCalled`, when badly overwhelmed),
**Defend** / **EliminateThreat** (both `!ThreatExists`, when threats exist),
**Patrol** (`AtPatrolPoint`, otherwise). Highest applicable one wins,
re-checked every tick. Defend and EliminateThreat share the same goal *state*
(kill the threats) but differ in execution - see "Holding the line" below.

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

## Holding the line (adapting to pressure)

Crank the spawn rate up and 3 agents can't win a war of attrition - so the
squad adapts. Once the threats clearly outnumber the living defenders
(>= 2x, with hysteresis so it doesn't flicker), every agent switches from
**EliminateThreat** to a **Defend** goal. Defend has the *same* symbolic goal
(`ThreatExists = false`), so the planner still produces the same
`Approach > Shoot` plans - the only difference is in *execution*: a "leash"
keeps a Defending agent from chasing a threat further than the heal-zone
radius from the outpost. They fall back, fight from home, and heal while they
fight, so they hold out far longer. Nobody scripted "retreat at 6 enemies";
it falls out of goal arbitration re-evaluated every tick. This is a nice
example of the same plan meaning different behaviour under a different
execution posture.

## Squad coordination (the layer on top of GOAP)

Vanilla GOAP agents plan *independently*, so ours used to all target the
nearest threat and dogpile it while other threats walked past to the outpost.
We added a thin **coordinator** that runs before the agents plan and hands each
one a *different* target (least-loaded threat, nearest first, with a little
stickiness to avoid flip-flopping). Each agent then still uses its own GOAP
plan to carry out the assignment. This is exactly how F.E.A.R. did it - a
squad manager on top, GOAP underneath - and it is a big, measurable win:
in our tests, at a steady swarm the outpost stayed at full health with
coordination on and was nearly destroyed with it off. Press **C** in the demo
to toggle it and show the difference live.

## Reinforcements as a planned action (not a spawn timer)

At extreme spawn rates even a holding squad loses - it's a numbers game. The
obvious fix is "spawn more defenders," but *how many units exist* is a
game/spawn concern, not something GOAP decides. So we made the **decision to
get help** a GOAP action instead of a timer.

`CallReinforcements` has precondition `AtOutpost` and effect
`ReinforcementsCalled`. A `CallForHelp` goal (state `ReinforcementsCalled=true`)
kicks in only when the squad is *badly* outnumbered (>= 3x). Now the agent
plans it: "fighting can't get me to safety, but there's an action whose effect
does, and I'm already holding at the outpost so its precondition is met -
cheapest plan: `CallReinforcements`." The label above the agent literally
changes from an attack plan to `CallReinforcements`. Executing it queues a
couple of fresh defenders (capped at 8 total) and starts a cooldown.

Two things fall out for free, without extra code:

- **A single caller emerges.** The instant one agent sets `ReinforcementsCalled`
  true, every other agent's `CallForHelp` goal is satisfied, so they go back to
  fighting. Nobody assigns "you make the call" - it's a consequence of a shared
  fact.
- **Escalating waves.** When the cooldown expires the fact resets to false; if
  the squad is still overwhelmed, someone calls again. Backup keeps arriving in
  waves that scale with how badly they're losing, until the cap.

This is the honest, GOAP-faithful way to answer "make them call for backup":
reinforcement is not a scripted event, it is an action the planner *chose*
because it was the cheapest way to reach the goal.

## Limitations / future work

- Facts are booleans only. Real systems (and F.E.A.R.) allow enums/numbers
  with per-fact distance heuristics.
- The coordinator is deliberately simple (greedy target assignment). Real
  squads assign *roles* (suppress, flank, advance), not just targets.
- Costs are hand-tuned constants; distance-based costs would make plans
  situational (fetch the gun only if it's close).
- The Defend "leash" is an execution-layer trick; a fuller solution would
  model position/cover in the planner itself.
