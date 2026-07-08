// Simulation.cpp - Part A of our GOAP demo.
// Implements the Simulation class from contract/Contract.h.
// All the "game" lives in here: agents, threats, weapons, the outpost,
// goal selection and action execution. The planner itself is in Planner.cpp
// and only ever sees the 8 symbolic facts from WorldState.h.

#include "../contract/Contract.h"
#include "WorldState.h"
#include "Action.h"
#include "Planner.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace goap {

// ---------------------------------------------------------------------------
// tuning values, all in one place so they are easy to tweak
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
    bool cheapMelee = false;
    bool replanEveryone = false;   // set when action costs change
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

    void resetWorld(bool firstTime)
    {
        time = 0.f;
        cheapMelee = false;
        replanEveryone = false;
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

    // squash the real world down into the 8 symbolic facts for one agent
    WorldState snapshotFor(const AgentData& a)
    {
        WorldState s;
        s.set(Fact::HasWeapon, a.hasWeapon);
        s.set(Fact::WeaponAvailable, !weapons.empty());
        s.set(Fact::ThreatExists, !threats.empty());

        const ThreatData* t = nullptr;
        for (auto& th : threats) {   // const-friendly nearest
            if (!t || dist(a.pos, th.pos) < dist(a.pos, t->pos))
                t = &th;
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

        // hysteresis: once in survive mode you count as "low health" until
        // you are healed back up to HEALED_HP, otherwise the goal would be
        // "done" the moment you regenerate 1 hp past the threshold
        s.set(Fact::LowHealth, a.surviveMode ? (a.hp < HEALED_HP) : (a.hp < LOW_HP));
        return s;
    }

    // priority order: Survive > EliminateThreat > Patrol
    std::string pickGoal(AgentData& a)
    {
        if (a.hp < LOW_HP)
            a.surviveMode = true;
        else if (a.hp >= HEALED_HP)
            a.surviveMode = false;

        if (a.surviveMode)
            return "Survive";
        if (!threats.empty())
            return "EliminateThreat";
        return "Patrol";
    }

    WorldState goalStateFor(const std::string& goal)
    {
        WorldState g;
        if (goal == "Survive")
            g.set(Fact::LowHealth, false);
        else if (goal == "EliminateThreat")
            g.set(Fact::ThreatExists, false);
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
            ThreatData* t = nearestThreat(a.pos);
            if (!t) { invalidatePlan(a, "no threats left"); return; }
            float stopDist = a.hasWeapon ? APPROACH_ARMED : APPROACH_UNARMED;
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

            ThreatData* t = nearestThreat(a.pos);
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
                a.pos = AGENT_SPAWNS[a.id - 1];
                a.phase = AgentPhase::Idle;
                a.goal = "None";
                a.surviveMode = false;
                addLog(agentName(a) + " respawned at the outpost");
            }
            return;
        }

        a.replanTimer -= dt;

        // 1) which goal matters most right now?
        std::string wanted = pickGoal(a);
        if (wanted != a.goal) {
            a.goal = wanted;
            addLog(agentName(a) + ": goal -> " + a.goal);
            a.plan.clear();
            a.step = 0;
            a.replanTimer = 0.f;  // goal changes replan immediately
        }

        WorldState now = snapshotFor(a);

        // 2) is the action we are in the middle of still doable?
        if (!a.plan.empty() && !now.satisfies(a.plan[a.step].pre)) {
            invalidatePlan(a, a.plan[a.step].name + " no longer valid");
        }

        // 3) no plan? make one (unless the goal is already satisfied)
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
            return;  // start executing next tick
        }

        // 4) run the current action
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
        // clamp dt so a debugger pause or laggy frame can't teleport things
        if (dt < 0.f) dt = 0.f;
        if (dt > 0.1f) dt = 0.1f;

        if (dt > 0.f) {
            time += dt;

            // costs changed -> everyone throws away their plan once
            if (replanEveryone) {
                replanEveryone = false;
                for (auto& a : agents) {
                    if (a.alive && !a.plan.empty())
                        invalidatePlan(a, "action costs changed");
                }
            }

            for (auto& a : agents)
                updateAgent(a, dt);
            updateThreats(dt);
            updateHealing(dt);
        }

        rebuildViews();
    }
};

// ---------------------------------------------------------------------------
// the public class just forwards everything to Impl
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
    ThreatData t;
    t.id = ++m_impl->threatCounter;
    t.pos = clampToWorld(pos);
    m_impl->threats.push_back(t);
    m_impl->addLog("threat T" + std::to_string(t.id) + " spawned");
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

const std::vector<AgentView>& Simulation::agents() const { return m_impl->agentViews; }
const std::vector<EntityView>& Simulation::entities() const { return m_impl->entityViews; }
const std::vector<std::string>& Simulation::eventLog() const { return m_impl->log; }
bool Simulation::meleeCheap() const { return m_impl->cheapMelee; }
Vec2 Simulation::worldSize() const { return { WORLD_W, WORLD_H }; }

} // namespace goap
