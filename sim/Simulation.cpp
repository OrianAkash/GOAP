// Simulation.cpp
// the whole game: agents, threats, weapons, outpost, goal selection and
// action execution. the planner itself is in Planner.cpp.

#include "../contract/Contract.h"
#include "WorldState.h"
#include "Action.h"
#include "Planner.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

namespace goap {

// ---------------------------------------------------------------------------
// tuning values
// ---------------------------------------------------------------------------
namespace {

const float WORLD_W = 1280.f;
const float WORLD_H = 720.f;

const Vec2  OUTPOST_POS = { 640.f, 360.f };
const float OUTPOST_MAX_HP = 500.f;

const Vec2 PATROL_POINTS[4] = { {640.f,140.f}, {1140.f,360.f}, {640.f,580.f}, {140.f,360.f} };
const Vec2 AGENT_SPAWNS[3]  = { {600.f,320.f}, {680.f,320.f}, {640.f,420.f} };
const Vec2 WEAPON_SPAWNS[2] = { {300.f,200.f}, {980.f,520.f} };

const float AGENT_MAX_HP  = 100.f;
const float AGENT_SPEED   = 120.f;   // px per second
const float THREAT_MAX_HP = 75.f;
const float THREAT_SPEED  = 60.f;

// body radii for soft collision separation (drawn radii are 10 / 9, these are
// a hair bigger so there's a visible gap). purely cosmetic - the planner never
// sees positions, so none of this touches the AI.
const float AGENT_RADIUS  = 11.f;
const float THREAT_RADIUS = 10.f;

const float SHOOT_RANGE = 180.f, SHOOT_DMG = 25.f, SHOOT_COOLDOWN = 0.8f;
const float MELEE_RANGE = 40.f,  MELEE_DMG = 15.f, MELEE_COOLDOWN = 0.6f;

const float THREAT_ATK_RANGE = 30.f, THREAT_DMG = 10.f, THREAT_COOLDOWN = 0.7f;
const float THREAT_VS_OUTPOST_RANGE = 70.f, THREAT_VS_OUTPOST_DMG = 25.f, THREAT_VS_OUTPOST_COOLDOWN = 1.0f;
const float THREAT_AGGRO_RANGE = 250.f;  // closer than this -> chases agents instead of outpost

const float HEAL_ZONE = 100.f;  // radius around the outpost where you heal
const float HEAL_RATE = 25.f;   // hp per second
const float LOW_HP = 35.f;      // below this -> Survive goal kicks in
const float HEALED_HP = 90.f;   // Survive goal ends here (hysteresis, see PLANNER.md)

const float PICKUP_RANGE = 25.f, PICKUP_TIME = 0.2f;
const float PATROL_ARRIVE = 8.f;
const float APPROACH_ARMED = 150.f;    // stop distance when we have a gun
const float APPROACH_UNARMED = 35.f;   // need to get close for melee
const float RESPAWN_TIME = 5.f;
const float REPLAN_COOLDOWN = 0.25f;   // don't replan more often than this
const float FAILED_PLAN_WAIT = 0.5f;

const int MAX_AUTO_THREATS = 40;       // safety cap so the spawn dial can't
                                       // spawn thousands and tank the framerate

const float DEFEND_LEASH = 100.f;      // while holding the line, agents won't
                                       // chase a threat further than this from
                                       // the outpost (== heal zone, so they heal
                                       // while they fight). matches HEAL_ZONE.

const int   MAX_AGENTS = 8;            // cap on total defenders (start 3 + backup)
const int   REINFORCE_COUNT = 2;       // defenders added per CallReinforcements
const float REINFORCE_COOLDOWN = 12.f; // seconds before backup can be called again
const float CALL_TIME = 0.5f;          // how long the "radio for backup" act takes

float dist(Vec2 a, Vec2 b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

Vec2 clampToWorld(Vec2 p)
{
    p.x = std::max(10.f, std::min(WORLD_W - 10.f, p.x));
    p.y = std::max(10.f, std::min(WORLD_H - 10.f, p.y));
    return p;
}

} // namespace

// ---------------------------------------------------------------------------
// internal data (what the planner does NOT see)
// ---------------------------------------------------------------------------

struct AgentData {
    int id = 0;
    Vec2 pos;
    float hp = AGENT_MAX_HP;
    bool alive = true;
    float respawnTimer = 0.f;
    bool hasWeapon = false;
    bool surviveMode = false;   // sticky flag for the Survive goal hysteresis
    int patrolIndex = 0;        // which patrol point we are heading to next

    std::string goal = "None";
    std::vector<Action> plan;
    std::size_t step = 0;
    AgentPhase phase = AgentPhase::Idle;
    int targetThreatId = 0;     // threat the coordinator assigned us (0 = none)

    float attackTimer = 0.f;    // counts down between shots/swings
    float actTimer = 0.f;       // generic "doing something" timer (pickup)
    float replanTimer = 0.f;    // counts down, no replanning while > 0
    bool hasTarget = false;
    Vec2 target;
};

struct ThreatData {
    int id = 0;
    Vec2 pos;
    float hp = THREAT_MAX_HP;
    float attackTimer = 0.f;
    float outpostTimer = 0.f;
};

struct WeaponData {
    int id = 0;
    Vec2 pos;
};

struct Simulation::Impl {
    float time = 0.f;
    bool cheapMelee = false;       // T key flips MeleeThreat cost 20 <-> 1
    float threatSpawnRate = 0.f;   // auto-spawned threats per second (0 = off)
    float spawnAccumulator = 0.f;  // fractional threats waiting to be spawned
    std::mt19937 rng{ 12345u };    // fixed seed -> a reset replays the same run
    bool replanEveryone = false;   // set when action costs change
    bool holdingLine = false;      // squad is outnumbered -> fall back and defend
    bool focusFire = true;         // coordinator splits targets (C toggles it)
    bool reinforcementsCalled = false; // backup radioed this wave (resets on cooldown)
    float reinforceCooldown = 0.f;     // counts down; when it hits 0 backup is available again
    int pendingReinforcements = 0;     // defenders to add after the update loop (avoids
                                       // growing the agents vector while iterating it)
    float outpostHp = OUTPOST_MAX_HP;
    bool overrunLogged = false;
    int threatCounter = 0;
    int weaponCounter = 0;

    std::vector<AgentData> agents;
    std::vector<ThreatData> threats;
    std::vector<WeaponData> weapons;
    std::vector<std::string> log;

    // cached copies handed out to the renderer
    std::vector<AgentView> agentViews;
    std::vector<EntityView> entityViews;

    // ---- helpers ----------------------------------------------------------

    void addLog(const std::string& msg)
    {
        char stamp[32];
        std::snprintf(stamp, sizeof(stamp), "[t=%.1f] ", time);
        log.push_back(std::string(stamp) + msg);
        if (log.size() > 50)
            log.erase(log.begin());
    }

    ThreatData* nearestThreat(Vec2 from)
    {
        ThreatData* best = nullptr;
        float bestDist = 0.f;
        for (auto& t : threats) {
            float d = dist(from, t.pos);
            if (!best || d < bestDist) {
                best = &t;
                bestDist = d;
            }
        }
        return best;
    }

    WeaponData* nearestWeapon(Vec2 from)
    {
        WeaponData* best = nullptr;
        float bestDist = 0.f;
        for (auto& w : weapons) {
            float d = dist(from, w.pos);
            if (!best || d < bestDist) {
                best = &w;
                bestDist = d;
            }
        }
        return best;
    }

    AgentData* nearestLivingAgent(Vec2 from)
    {
        AgentData* best = nullptr;
        float bestDist = 0.f;
        for (auto& a : agents) {
            if (!a.alive)
                continue;
            float d = dist(from, a.pos);
            if (!best || d < bestDist) {
                best = &a;
                bestDist = d;
            }
        }
        return best;
    }

    int countLivingAgents()
    {
        int n = 0;
        for (auto& a : agents)
            if (a.alive) n++;
        return n;
    }

    // where an agent (re)spawns: the 3 original posts, then a small ring
    // around the outpost for reinforcements (ids 4+)
    Vec2 spawnPointFor(int id)
    {
        if (id >= 1 && id <= 3)
            return AGENT_SPAWNS[id - 1];
        float ang = (float)id * 2.399963f;  // golden angle -> they don't stack
        return clampToWorld({ OUTPOST_POS.x + std::cos(ang) * 34.f,
                              OUTPOST_POS.y + std::sin(ang) * 34.f });
    }

    // "we can't win this by fighting" - the trigger for calling backup.
    // (3x is stricter than the 2x that starts the fall-back to Defend.)
    bool squadOverwhelmed()
    {
        int living = countLivingAgents();
        return living > 0 && (int)threats.size() >= living * 3;
    }

    // actually radio for backup: queue new defenders (added after the update
    // loop so we never resize the agents vector while iterating it)
    void callReinforcements(const AgentData& caller)
    {
        reinforcementsCalled = true;
        reinforceCooldown = REINFORCE_COOLDOWN;
        int room = MAX_AGENTS - (int)agents.size();
        int add = room < REINFORCE_COUNT ? room : REINFORCE_COUNT;
        if (add < 0) add = 0;
        pendingReinforcements += add;
        if (add > 0)
            addLog(agentName(caller) + " called reinforcements! (+" +
                   std::to_string(add) + " defenders inbound)");
        else
            addLog(agentName(caller) + " called reinforcements (none available)");
    }

    ThreatData* threatById(int id)
    {
        for (auto& t : threats)
            if (t.id == id) return &t;
        return nullptr;
    }

    // which threat this agent should be fighting: its coordinator-assigned
    // target if it still exists, otherwise just the nearest one
    ThreatData* engageTarget(const AgentData& a)
    {
        ThreatData* t = threatById(a.targetThreatId);
        if (t) return t;
        return nearestThreat(a.pos);
    }

    // outnumbered check, 2x to enter / 1x to leave so it doesn't flicker
    void updateHoldStatus()
    {
        int living = countLivingAgents();
        int th = (int)threats.size();
        if (living <= 0) { holdingLine = false; return; }
        if (th >= living * 2)      holdingLine = true;
        else if (th <= living)     holdingLine = false;
    }

    // coordinator, runs before the agents plan: hand each defender its own
    // threat so they don't all dogpile the nearest one
    void coordinateTargets()
    {
        if (threats.empty()) {
            for (auto& a : agents) a.targetThreatId = 0;
            return;
        }
        if (!focusFire) {
            // coordination off -> target 0 makes engageTarget fall back to the
            // nearest threat, i.e. everyone dogpiles (the "before" case)
            for (auto& a : agents)
                a.targetThreatId = 0;
            return;
        }

        std::map<int, int> load;  // how many agents already sent to each threat
        for (auto& a : agents) {
            if (!a.alive) { a.targetThreatId = 0; continue; }
            ThreatData* best = nullptr;
            float bestScore = 0.f;
            for (auto& t : threats) {
                // prefer the least-loaded threat, then the nearest; a small
                // bonus for keeping our current target avoids frame-to-frame
                // flip-flopping when two threats are about the same distance
                float score = load[t.id] * 100000.f + dist(a.pos, t.pos);
                if (t.id == a.targetThreatId) score -= 60.f;
                if (!best || score < bestScore) { best = &t; bestScore = score; }
            }
            a.targetThreatId = best ? best->id : 0;
            if (best) load[best->id]++;
        }
    }

    // pick a random point just inside one of the four map edges, so
    // auto-spawned threats stream in from around the border
    Vec2 randomEdgePos()
    {
        std::uniform_real_distribution<float> ox(40.f, WORLD_W - 40.f);
        std::uniform_real_distribution<float> oy(40.f, WORLD_H - 40.f);
        switch (rng() % 4) {
        case 0:  return { ox(rng), 40.f };            // top
        case 1:  return { WORLD_W - 40.f, oy(rng) };  // right
        case 2:  return { ox(rng), WORLD_H - 40.f };  // bottom
        default: return { 40.f, oy(rng) };            // left
        }
    }

    // add a threat without rebuilding views (tick rebuilds once at the end)
    void spawnThreatAt(Vec2 pos, const char* note)
    {
        ThreatData t;
        t.id = ++threatCounter;
        t.pos = clampToWorld(pos);
        threats.push_back(t);
        addLog("threat T" + std::to_string(t.id) + " spawned" + std::string(note));
    }

    void resetWorld(bool firstTime)
    {
        time = 0.f;
        cheapMelee = false;
        threatSpawnRate = 0.f;
        spawnAccumulator = 0.f;
        rng.seed(12345u);
        replanEveryone = false;
        holdingLine = false;
        focusFire = true;
        reinforcementsCalled = false;
        reinforceCooldown = 0.f;
        pendingReinforcements = 0;
        outpostHp = OUTPOST_MAX_HP;
        overrunLogged = false;
        threatCounter = 0;
        weaponCounter = 0;
        agents.clear();
        threats.clear();
        weapons.clear();
        log.clear();

        for (int i = 0; i < 3; i++) {
            AgentData a;
            a.id = i + 1;
            a.pos = AGENT_SPAWNS[i];
            a.patrolIndex = a.id % 4;  // spread them over different points
            agents.push_back(a);
        }
        for (int i = 0; i < 2; i++) {
            WeaponData w;
            w.id = ++weaponCounter;
            w.pos = WEAPON_SPAWNS[i];
            weapons.push_back(w);
        }

        addLog(firstTime ? "demo start - 3 agents defending the outpost"
                         : "simulation reset");
    }

    // ---- planning side ----------------------------------------------------

    // snapshot the world into planner facts, from this agent's point of view
    WorldState snapshotFor(const AgentData& a)
    {
        WorldState s;
        s.set(Fact::HasWeapon, a.hasWeapon);
        s.set(Fact::WeaponAvailable, !weapons.empty());
        s.set(Fact::ThreatExists, !threats.empty());

        // range is measured against the threat we'd actually engage (our
        // assigned target if any, else the nearest) so the plan matches what
        // the execution layer will do
        const ThreatData* t = threatById(a.targetThreatId);
        if (!t) {
            for (auto& th : threats) {   // fall back to nearest
                if (!t || dist(a.pos, th.pos) < dist(a.pos, t->pos))
                    t = &th;
            }
        }
        float attackRange = a.hasWeapon ? SHOOT_RANGE : MELEE_RANGE;
        s.set(Fact::ThreatInRange, t != nullptr && dist(a.pos, t->pos) <= attackRange);

        const WeaponData* w = nullptr;
        for (auto& wp : weapons) {
            if (!w || dist(a.pos, wp.pos) < dist(a.pos, w->pos))
                w = &wp;
        }
        s.set(Fact::NextToWeapon, w != nullptr && dist(a.pos, w->pos) <= PICKUP_RANGE);

        s.set(Fact::AtPatrolPoint, dist(a.pos, PATROL_POINTS[a.patrolIndex]) <= PATROL_ARRIVE);
        s.set(Fact::AtOutpost, dist(a.pos, OUTPOST_POS) <= HEAL_ZONE);

        // hysteresis: stays "low health" until healed to HEALED_HP, or the
        // Survive goal would complete the moment we cross the threshold
        s.set(Fact::LowHealth, a.surviveMode ? (a.hp < HEALED_HP) : (a.hp < LOW_HP));

        // squad-wide fact: has backup already been radioed this wave?
        s.set(Fact::ReinforcementsCalled, reinforcementsCalled);
        return s;
    }

    // priority order: Survive > Defend > EliminateThreat > Patrol.
    // Defend is "kill the threats, but from home" - it only differs from
    // EliminateThreat in how it is executed (see the leash in executeAction).
    std::string pickGoal(AgentData& a)
    {
        if (a.hp < LOW_HP)
            a.surviveMode = true;
        else if (a.hp >= HEALED_HP)
            a.surviveMode = false;

        if (a.surviveMode)
            return "Survive";
        if (!threats.empty()) {
            // dire and haven't called backup yet -> go radio for reinforcements
            if (squadOverwhelmed() && !reinforcementsCalled &&
                (int)agents.size() < MAX_AGENTS)
                return "CallForHelp";
            return holdingLine ? "Defend" : "EliminateThreat";
        }
        return "Patrol";
    }

    WorldState goalStateFor(const std::string& goal)
    {
        WorldState g;
        if (goal == "Survive")
            g.set(Fact::LowHealth, false);
        else if (goal == "EliminateThreat" || goal == "Defend")
            g.set(Fact::ThreatExists, false);
        else if (goal == "CallForHelp")
            g.set(Fact::ReinforcementsCalled, true);
        else if (goal == "Patrol")
            g.set(Fact::AtPatrolPoint, true);
        return g;
    }

    std::string planToString(const std::vector<Action>& plan)
    {
        std::string out;
        for (std::size_t i = 0; i < plan.size(); i++) {
            if (i > 0)
                out += " > ";
            out += plan[i].name;
        }
        return out;
    }

    std::string agentName(const AgentData& a)
    {
        return "A" + std::to_string(a.id);
    }

    // ---- execution side ----------------------------------------------------

    void moveToward(AgentData& a, Vec2 dest, float dt)
    {
        a.phase = AgentPhase::Moving;
        a.hasTarget = true;
        a.target = dest;
        float d = dist(a.pos, dest);
        if (d < 0.001f)
            return;
        float step = AGENT_SPEED * dt;
        if (step > d)
            step = d;
        a.pos.x += (dest.x - a.pos.x) / d * step;
        a.pos.y += (dest.y - a.pos.y) / d * step;
    }

    void completeStep(AgentData& a)
    {
        a.step++;
        a.actTimer = 0.f;
        if (a.step >= a.plan.size()) {
            // plan finished - goal re-evaluation next tick decides what's next
            a.plan.clear();
            a.step = 0;
        }
    }

    void invalidatePlan(AgentData& a, const std::string& reason)
    {
        addLog(agentName(a) + ": REPLAN (" + reason + ")");
        a.plan.clear();
        a.step = 0;
        a.replanTimer = REPLAN_COOLDOWN;
    }

    void killThreat(ThreatData* t, const AgentData& killer)
    {
        addLog("T" + std::to_string(t->id) + " destroyed by " + agentName(killer));
        for (std::size_t i = 0; i < threats.size(); i++) {
            if (&threats[i] == t) {
                threats.erase(threats.begin() + i);
                break;
            }
        }
    }

    void executeAction(AgentData& a, float dt)
    {
        const Action& cur = a.plan[a.step];
        a.attackTimer -= dt;

        if (cur.name == "GoToWeapon") {
            WeaponData* w = nearestWeapon(a.pos);
            if (!w) { invalidatePlan(a, "weapon is gone"); return; }
            moveToward(a, w->pos, dt);
            if (dist(a.pos, w->pos) <= PICKUP_RANGE)
                completeStep(a);
        }
        else if (cur.name == "PickUpWeapon") {
            WeaponData* w = nearestWeapon(a.pos);
            if (!w || dist(a.pos, w->pos) > PICKUP_RANGE) {
                invalidatePlan(a, "weapon was taken");  // another agent beat us to it
                return;
            }
            a.phase = AgentPhase::Acting;
            a.hasTarget = true;
            a.target = w->pos;
            a.actTimer += dt;
            if (a.actTimer >= PICKUP_TIME) {
                for (std::size_t i = 0; i < weapons.size(); i++) {
                    if (&weapons[i] == w) {
                        weapons.erase(weapons.begin() + i);
                        break;
                    }
                }
                a.hasWeapon = true;
                addLog(agentName(a) + " picked up a weapon");
                completeStep(a);
            }
        }
        else if (cur.name == "ApproachThreat") {
            ThreatData* t = engageTarget(a);
            if (!t) { invalidatePlan(a, "no threats left"); return; }
            float stopDist = a.hasWeapon ? APPROACH_ARMED : APPROACH_UNARMED;

            if (a.goal == "Defend") {
                // hold the line: fight from the outpost, don't chase into the
                // field. if we drifted out, walk home first; otherwise advance
                // toward the threat but never past the leash (so we keep
                // healing while we fight). the threats come to us.
                if (dist(a.pos, OUTPOST_POS) > DEFEND_LEASH + 2.f) {
                    moveToward(a, OUTPOST_POS, dt);
                    return;
                }
                if (dist(a.pos, t->pos) <= stopDist) { completeStep(a); return; }
                Vec2 dest = t->pos;
                float dh = dist(OUTPOST_POS, dest);
                if (dh > DEFEND_LEASH) {   // clamp target onto the leash circle
                    dest.x = OUTPOST_POS.x + (dest.x - OUTPOST_POS.x) / dh * DEFEND_LEASH;
                    dest.y = OUTPOST_POS.y + (dest.y - OUTPOST_POS.y) / dh * DEFEND_LEASH;
                }
                moveToward(a, dest, dt);
                if (dist(a.pos, t->pos) <= stopDist)
                    completeStep(a);
                return;
            }

            if (dist(a.pos, t->pos) <= stopDist) {
                completeStep(a);
                return;
            }
            moveToward(a, t->pos, dt);
        }
        else if (cur.name == "ShootThreat" || cur.name == "MeleeThreat") {
            bool melee = (cur.name == "MeleeThreat");
            float range = melee ? MELEE_RANGE : SHOOT_RANGE;
            float dmg = melee ? MELEE_DMG : SHOOT_DMG;
            float cooldown = melee ? MELEE_COOLDOWN : SHOOT_COOLDOWN;

            ThreatData* t = engageTarget(a);
            if (!t) { completeStep(a); return; }  // someone else got the kill
            a.phase = AgentPhase::Acting;
            a.hasTarget = true;
            a.target = t->pos;
            if (dist(a.pos, t->pos) > range) {
                // target moved away - the precondition check next tick will
                // notice ThreatInRange is false and replan (approach again)
                return;
            }
            if (a.attackTimer <= 0.f) {
                a.attackTimer = cooldown;
                t->hp -= dmg;
                if (t->hp <= 0.f) {
                    killThreat(t, a);
                    completeStep(a);
                }
            }
        }
        else if (cur.name == "GoToPatrolPoint") {
            Vec2 dest = PATROL_POINTS[a.patrolIndex];
            moveToward(a, dest, dt);
            if (dist(a.pos, dest) <= PATROL_ARRIVE) {
                // move on to the next point so the next Patrol plan has
                // somewhere new to go
                a.patrolIndex = (a.patrolIndex + 1) % 4;
                completeStep(a);
            }
        }
        else if (cur.name == "RetreatToOutpost") {
            moveToward(a, OUTPOST_POS, dt);
            if (dist(a.pos, OUTPOST_POS) <= HEAL_ZONE)
                completeStep(a);
        }
        else if (cur.name == "HealAtOutpost") {
            a.phase = AgentPhase::Acting;
            a.hasTarget = false;
            // the heal zone (see tick) raises our hp, we just wait
            if (a.hp >= HEALED_HP)
                completeStep(a);
        }
        else if (cur.name == "CallReinforcements") {
            a.phase = AgentPhase::Acting;
            a.hasTarget = false;
            a.actTimer += dt;
            if (a.actTimer >= CALL_TIME) {
                if (!reinforcementsCalled)     // someone may have just beaten us to it
                    callReinforcements(a);
                completeStep(a);
            }
        }
        else {
            // unknown action should never happen, but just in case
            invalidatePlan(a, "unknown action " + cur.name);
        }
    }

    void updateAgent(AgentData& a, float dt)
    {
        if (!a.alive) {
            a.respawnTimer -= dt;
            if (a.respawnTimer <= 0.f) {
                a.alive = true;
                a.hp = AGENT_MAX_HP;
                a.pos = spawnPointFor(a.id);
                a.phase = AgentPhase::Idle;
                a.goal = "None";
                a.surviveMode = false;
                addLog(agentName(a) + " respawned at the outpost");
            }
            return;
        }

        a.replanTimer -= dt;

        // 1) pick goal
        std::string wanted = pickGoal(a);
        if (wanted != a.goal) {
            a.goal = wanted;
            addLog(agentName(a) + ": goal -> " + a.goal);
            a.plan.clear();
            a.step = 0;
            a.replanTimer = 0.f;  // goal changes replan immediately
        }

        WorldState now = snapshotFor(a);

        // 2) current action still valid?
        if (!a.plan.empty() && !now.satisfies(a.plan[a.step].pre)) {
            invalidatePlan(a, a.plan[a.step].name + " no longer valid");
        }

        // 3) plan if needed
        if (a.plan.empty()) {
            a.hasTarget = false;
            if (now.satisfies(goalStateFor(a.goal))) {
                a.phase = AgentPhase::Idle;  // e.g. just finished healing
                return;
            }
            if (a.replanTimer > 0.f) {
                a.phase = AgentPhase::Idle;  // short breather between replans
                return;
            }
            a.phase = AgentPhase::Planning;
            a.plan = makePlan(now, goalStateFor(a.goal), buildActionList(cheapMelee));
            a.step = 0;
            if (a.plan.empty()) {
                addLog(agentName(a) + ": no plan found for " + a.goal);
                a.replanTimer = FAILED_PLAN_WAIT;
                a.phase = AgentPhase::Idle;
                return;
            }
            addLog(agentName(a) + ": plan = " + planToString(a.plan));
            return;  // execute from next tick
        }

        // 4) execute
        executeAction(a, dt);
    }

    void updateThreats(float dt)
    {
        for (auto& t : threats) {
            t.attackTimer -= dt;
            t.outpostTimer -= dt;

            AgentData* victim = nearestLivingAgent(t.pos);
            bool chaseAgent = victim && dist(t.pos, victim->pos) <= THREAT_AGGRO_RANGE;

            if (chaseAgent) {
                float d = dist(t.pos, victim->pos);
                if (d > THREAT_ATK_RANGE) {
                    float step = THREAT_SPEED * dt;
                    if (step > d) step = d;
                    t.pos.x += (victim->pos.x - t.pos.x) / d * step;
                    t.pos.y += (victim->pos.y - t.pos.y) / d * step;
                }
                else if (t.attackTimer <= 0.f) {
                    t.attackTimer = THREAT_COOLDOWN;
                    victim->hp -= THREAT_DMG;
                    if (victim->hp <= 0.f) {
                        victim->hp = 0.f;
                        victim->alive = false;
                        victim->phase = AgentPhase::Dead;
                        victim->hasWeapon = false;   // weapon is lost, tough luck
                        victim->plan.clear();
                        victim->step = 0;
                        victim->goal = "None";
                        victim->hasTarget = false;
                        victim->respawnTimer = RESPAWN_TIME;
                        addLog(agentName(*victim) + " died, respawn in 5s");
                    }
                }
            }
            else {
                // nobody nearby -> march on the outpost
                float d = dist(t.pos, OUTPOST_POS);
                if (d > THREAT_VS_OUTPOST_RANGE) {
                    float step = THREAT_SPEED * dt;
                    if (step > d) step = d;
                    t.pos.x += (OUTPOST_POS.x - t.pos.x) / d * step;
                    t.pos.y += (OUTPOST_POS.y - t.pos.y) / d * step;
                }
                else if (t.outpostTimer <= 0.f) {
                    t.outpostTimer = THREAT_VS_OUTPOST_COOLDOWN;
                    outpostHp -= THREAT_VS_OUTPOST_DMG;
                    if (outpostHp <= 0.f) {
                        outpostHp = 0.f;
                        if (!overrunLogged) {
                            overrunLogged = true;
                            addLog("OUTPOST OVERRUN! (demo keeps running)");
                        }
                    }
                }
            }
        }
    }

    // soft collision: nudge overlapping bodies apart, cosmetic only
    void resolveCollisions()
    {
        struct Body { Vec2* pos; float r; };
        std::vector<Body> bodies;
        for (auto& a : agents)
            if (a.alive) bodies.push_back({ &a.pos, AGENT_RADIUS });
        for (auto& t : threats)
            bodies.push_back({ &t.pos, THREAT_RADIUS });

        const int ITER = 2;
        for (int it = 0; it < ITER; it++) {
            for (std::size_t i = 0; i < bodies.size(); i++) {
                for (std::size_t j = i + 1; j < bodies.size(); j++) {
                    Vec2& p = *bodies[i].pos;
                    Vec2& q = *bodies[j].pos;
                    float mind = bodies[i].r + bodies[j].r;
                    float dx = q.x - p.x, dy = q.y - p.y;
                    float d2 = dx * dx + dy * dy;
                    if (d2 >= mind * mind) continue;
                    float d = std::sqrt(d2);
                    if (d < 0.0001f) {   // exactly coincident -> deterministic nudge
                        dx = (float)(((int)i % 3) - 1);
                        dy = (float)(((int)j % 3) - 1);
                        d = std::sqrt(dx * dx + dy * dy);
                        if (d < 0.0001f) { dx = 1.f; dy = 0.f; d = 1.f; }
                    }
                    float push = (mind - d) * 0.5f;
                    float ux = dx / d, uy = dy / d;
                    p.x -= ux * push; p.y -= uy * push;
                    q.x += ux * push; q.y += uy * push;
                }
            }
        }
        for (auto& a : agents)
            if (a.alive) a.pos = clampToWorld(a.pos);
        for (auto& t : threats)
            t.pos = clampToWorld(t.pos);
    }

    void updateHealing(float dt)
    {
        for (auto& a : agents) {
            if (a.alive && dist(a.pos, OUTPOST_POS) <= HEAL_ZONE) {
                a.hp += HEAL_RATE * dt;
                if (a.hp > AGENT_MAX_HP)
                    a.hp = AGENT_MAX_HP;
            }
        }
    }

    // ---- views for the renderer -------------------------------------------

    void rebuildViews()
    {
        agentViews.clear();
        for (auto& a : agents) {
            AgentView v;
            v.id = a.id;
            v.pos = a.pos;
            v.health = a.hp;
            v.hasWeapon = a.hasWeapon;
            v.phase = a.alive ? a.phase : AgentPhase::Dead;
            v.goalName = a.goal;
            for (auto& act : a.plan)
                v.plan.push_back(act.name);
            v.currentStep = a.step;
            v.hasTarget = a.alive && a.hasTarget;
            v.targetPos = a.target;
            agentViews.push_back(v);
        }

        entityViews.clear();
        {
            EntityView e;
            e.id = 0;
            e.type = EntityType::Outpost;
            e.pos = OUTPOST_POS;
            e.health = outpostHp;
            e.maxHealth = OUTPOST_MAX_HP;
            entityViews.push_back(e);
        }
        for (int i = 0; i < 4; i++) {
            EntityView e;
            e.id = 1 + i;
            e.type = EntityType::PatrolPoint;
            e.pos = PATROL_POINTS[i];
            entityViews.push_back(e);
        }
        for (auto& w : weapons) {
            EntityView e;
            e.id = 100 + w.id;
            e.type = EntityType::Weapon;
            e.pos = w.pos;
            entityViews.push_back(e);
        }
        for (auto& t : threats) {
            EntityView e;
            e.id = 200 + t.id;
            e.type = EntityType::Threat;
            e.pos = t.pos;
            e.health = t.hp;
            e.maxHealth = THREAT_MAX_HP;
            entityViews.push_back(e);
        }
    }

    void tick(float dt)
    {
        // clamp dt so a lag spike can't teleport things
        if (dt < 0.f) dt = 0.f;
        if (dt > 0.1f) dt = 0.1f;

        if (dt > 0.f) {
            time += dt;

            // pressure dial: drip in threats at the chosen rate
            if (threatSpawnRate > 0.f) {
                spawnAccumulator += threatSpawnRate * dt;
                while (spawnAccumulator >= 1.f) {
                    spawnAccumulator -= 1.f;
                    if ((int)threats.size() < MAX_AUTO_THREATS)
                        spawnThreatAt(randomEdgePos(), " (pressure)");
                }
            }

            // costs changed -> everyone replans once
            if (replanEveryone) {
                replanEveryone = false;
                for (auto& a : agents) {
                    if (a.alive && !a.plan.empty())
                        invalidatePlan(a, "action costs changed");
                }
            }

            // reinforcement backup becomes available again once the cooldown
            // runs out (lets the squad call another wave if still overwhelmed)
            if (reinforceCooldown > 0.f) {
                reinforceCooldown -= dt;
                if (reinforceCooldown <= 0.f)
                    reinforcementsCalled = false;
            }

            // squad-level thinking happens before the individual agents plan:
            // decide whether we are outnumbered, then split up the targets
            updateHoldStatus();
            coordinateTargets();

            for (auto& a : agents)
                updateAgent(a, dt);

            // add any queued reinforcements now that we're done iterating agents
            while (pendingReinforcements > 0 && (int)agents.size() < MAX_AGENTS) {
                pendingReinforcements--;
                AgentData na;
                na.id = (int)agents.size() + 1;
                na.pos = spawnPointFor(na.id);
                na.patrolIndex = na.id % 4;
                agents.push_back(na);
                addLog(agentName(na) + " arrived as reinforcement");
            }
            pendingReinforcements = 0;

            updateThreats(dt);
            updateHealing(dt);
            resolveCollisions();   // keep bodies from overlapping (cosmetic)
        }

        rebuildViews();
    }
};

// ---------------------------------------------------------------------------
// public class forwards to Impl
// ---------------------------------------------------------------------------

Simulation::Simulation() : m_impl(new Impl())
{
    m_impl->resetWorld(true);
    m_impl->rebuildViews();
}

Simulation::~Simulation() = default;

void Simulation::tick(float dtSeconds) { m_impl->tick(dtSeconds); }

void Simulation::reset()
{
    m_impl->resetWorld(false);
    m_impl->rebuildViews();
}

void Simulation::spawnThreat(Vec2 pos)
{
    m_impl->spawnThreatAt(pos, "");
    m_impl->rebuildViews();
}

void Simulation::spawnWeapon(Vec2 pos)
{
    WeaponData w;
    w.id = ++m_impl->weaponCounter;
    w.pos = clampToWorld(pos);
    m_impl->weapons.push_back(w);
    m_impl->addLog("weapon dropped in the field");
    m_impl->rebuildViews();
}

void Simulation::removeAllWeapons()
{
    m_impl->weapons.clear();
    m_impl->addLog("all ground weapons removed");
    m_impl->rebuildViews();
}

void Simulation::injureNearestAgent(Vec2 pos)
{
    AgentData* a = m_impl->nearestLivingAgent(pos);
    if (!a) {
        m_impl->addLog("no living agent to injure");
    } else {
        a->hp = 20.f;
        m_impl->addLog(m_impl->agentName(*a) + " injured (hp 20)");
    }
    m_impl->rebuildViews();
}

void Simulation::toggleMeleeCost()
{
    m_impl->cheapMelee = !m_impl->cheapMelee;
    m_impl->replanEveryone = true;
    m_impl->addLog(m_impl->cheapMelee ? "Melee cost set to 1 (cheap!)"
                                      : "Melee cost set to 20 (normal)");
    m_impl->rebuildViews();
}

void Simulation::setThreatSpawnRate(float perSecond)
{
    if (perSecond < 0.f) perSecond = 0.f;
    // pushed every frame from the slider, so no logging (would spam) and no
    // rebuild here - tick() rebuilds. drop the leftover fraction when turned off.
    if (perSecond == 0.f)
        m_impl->spawnAccumulator = 0.f;
    m_impl->threatSpawnRate = perSecond;
}

void Simulation::toggleFocusFire()
{
    m_impl->focusFire = !m_impl->focusFire;
    m_impl->addLog(m_impl->focusFire ? "squad coordination ON (split targets)"
                                     : "squad coordination OFF (dogpile nearest)");
    m_impl->rebuildViews();
}

const std::vector<AgentView>& Simulation::agents() const { return m_impl->agentViews; }
const std::vector<EntityView>& Simulation::entities() const { return m_impl->entityViews; }
const std::vector<std::string>& Simulation::eventLog() const { return m_impl->log; }
bool Simulation::meleeCheap() const { return m_impl->cheapMelee; }
float Simulation::threatSpawnRate() const { return m_impl->threatSpawnRate; }
bool Simulation::focusFire() const { return m_impl->focusFire; }
bool Simulation::holdingLine() const { return m_impl->holdingLine; }
Vec2 Simulation::worldSize() const { return { WORLD_W, WORLD_H }; }

} // namespace goap
