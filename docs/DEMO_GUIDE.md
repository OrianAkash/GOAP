# Demo script (for the presentation)

Rough order that shows everything off in ~3 minutes. Worth doing a dry run
before presenting.

1. **Launch.** Read the overlay out loud (it explains GOAP in 2 sentences),
   press a key. The agents pick the `Patrol` goal and plan `GoToPatrolPoint`.
   Point out: even this is a *planned* action, not a scripted loop.

2. **Left-click somewhere far from the agents.** A threat spawns, every agent
   switches goal to `EliminateThreat`. The agents are unarmed, so the planner
   produces the 4-step plan:
   `GoToWeapon > PickUpWeapon > ApproachThreat > ShootThreat`.
   Say it clearly: *nobody wrote "if unarmed fetch a weapon first" anywhere in
   the code* - the planner found that chain because fetching (cost 10) is
   cheaper than melee (cost 23).

3. **While an agent is still walking to a weapon, press W** (remove weapons).
   The log prints a REPLAN line and the plan flips to
   `ApproachThreat > MeleeThreat` - the only plan that still works.

4. **Press T** (melee cost 20 -> 1). Everyone replans; now even agents with
   guns choose `MeleeThreat`, because 3+1 beats 3+2. One constant changed,
   completely different squad behaviour. Press T again to flip it back.

5. **Hover the mouse near an agent and press H.** It drops to 20 hp, the
   `Survive` goal (highest priority) takes over:
   `RetreatToOutpost > HealAtOutpost`. It runs home, heals inside the green
   ring to 90hp, then goes right back to fighting/patrolling.

6. **Go wild.** Spawn 10 threats, pause with SPACE to narrate the log,
   R to reset. If agents die they respawn at the outpost after 5s (unarmed -
   watch them fetch weapons again).

Questions we should be ready for (see docs/PLANNER.md for the answers):
- why regressive (backwards) search instead of forwards?
- what stops silly plans like picking up a weapon you walked away from?
- what happens when no plan exists?
- how does this compare to FSMs / behaviour trees?
