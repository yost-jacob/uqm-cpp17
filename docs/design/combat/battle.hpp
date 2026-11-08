#ifndef UQM_DESIGN_COMBAT_BATTLE_HPP_
#define UQM_DESIGN_COMBAT_BATTLE_HPP_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

// Declaration-only design sketch. Not linked into gameplay.
namespace uqm::design::combat {
using Tick = std::uint64_t;
struct ShipId { std::uint64_t battle, serial; };
struct ProjectileId { std::uint64_t battle, serial; };
struct TeamId { unsigned value; };
struct Vector2 { double x, y; };
struct ShipCommands {
    bool turnLeft = false, turnRight = false, thrust = false;
    bool firePrimary = false, useSpecial = false;
};
struct ShipDefinition; // Validated JSON properties; flagship loadout resolved.
struct ShipAssets;     // Separate RAII asset owner; absent in headless simulation.
struct ShipSnapshot {
    ShipId id;
    TeamId team;
    Vector2 position, velocity;
    double facing;
    unsigned crew, energy;
    // Capability and cooldown observations must be added before AI migration.
};
struct ProjectileSnapshot {
    ProjectileId id;
    ShipId source;
    TeamId team;
    Vector2 position, velocity;
    unsigned remainingTicks;
};
struct CombatView {
    Tick tick;
    ShipId self;
    std::vector<ShipSnapshot> ships;
    std::vector<ProjectileSnapshot> projectiles;
};
struct Hit {
    ShipId source; // Attribution survives source destruction; never dereference it.
    unsigned damage;
    // Contact position, impulse and special effects need a later collision audit.
};
struct DamageResult { unsigned crewLost; bool destroyed; };
struct BattleResult {
    std::optional<TeamId> winner; // nullopt means a draw.
    std::vector<ShipSnapshot> finalShips; // Includes destroyed combatants.
    Tick duration;
};
struct BattleConfig {
    std::uint64_t identity; // Unique among battles whose handles can coexist.
    std::uint32_t seed;
    // Fixed simulation cadence and arena dimensions remain legacy-defined initially.
};

class Battle;
class Projectile;

// Borrowed for one callback, gameplay-thread only. Do not retain this context.
// No campaign, graphics device or mutable container access is exposed.
class BattleContext {
public:
    Tick tick() const noexcept;
    std::optional<ShipSnapshot> ship(ShipId id) const;
    std::optional<ProjectileSnapshot> projectile(ProjectileId id) const;
    std::uint32_t random(); // Advances only this battle's RNG.

    // Reject null. Assign a battle-local ID and queue insertion for a safe phase.
    // Spawned projectiles first simulate on the following tick.
    ProjectileId spawn(std::unique_ptr<Projectile> projectile);
    // Queue a command; return false for another battle, expired or removed ID.
    // Repeated requests before resolution coalesce to one detonation.
    bool detonate(ProjectileId id);
private:
    friend class Battle;
    explicit BattleContext(Battle &battle);
    Battle &battle_;
};

class ShipController {
public:
    virtual ~ShipController() = default;
    // Read-only observation for this tick. Never retain references into view.
    // Network adapters return buffered input; no waiting for sockets here.
    virtual ShipCommands commands(const CombatView &view) = 0;
};

class Ship {
public:
    virtual ~Ship() = default;
    // Definition is immutable and remains valid for this instance's lifetime.
    virtual const ShipDefinition &definition() const noexcept = 0;
    virtual ShipSnapshot snapshot() const = 0;
    // Exactly once before common movement/collisions. Commands are held states;
    // retain prior commands inside the instance when release detection is needed.
    virtual void beginTick(BattleContext &context, const ShipCommands &commands) = 0;
    // Exactly once after collision resolution for a surviving ship.
    virtual void endTick(BattleContext &context) = 0;
    // Apply resolved contact damage and defenses; never mutate campaign state.
    virtual DamageResult receiveHit(BattleContext &context, const Hit &hit) = 0;
};

// Battle ownership lets a projectile survive the ship that launched it.
// Straight/seeking implementations can share motion functions and definition data.
class Projectile {
public:
    virtual ~Projectile() = default;
    virtual ProjectileSnapshot snapshot() const = 0;
    // Guidance/ability update before common motion and collision processing.
    // Target references use IDs; a missing target needs an explicit fallback.
    virtual void beginTick(BattleContext &context) = 0;
    // Called at most once when detonation is resolved, including remote release.
    // May queue fragments. Battle retires this projectile after the callback.
    virtual void detonate(BattleContext &context) = 0;
    // Collision-specific behavior and physical integration are intentionally
    // unspecified here until the legacy element/collision pipeline is audited.
};

// One independent simulation. No window, sound device, campaign owner or pacing.
// All methods are confined to its simulation thread. Incomplete types are owned
// through out-of-line construction/destruction. This is not yet an implementation.
class Battle {
public:
    // Validate identity/configuration. Own RNG and all mutable simulation state.
    explicit Battle(BattleConfig config);
    ~Battle() noexcept; // Cleanup only; no training or campaign writeback.
    Battle(const Battle &) = delete;
    Battle &operator=(const Battle &) = delete;
    Battle(Battle &&) = delete;
    Battle &operator=(Battle &&) = delete;

    // Before the first tick only. Both owners must be non-null. Ship construction
    // supplies initial crew/loadout; no campaign access occurs through Battle.
    // IDs are assigned by Battle and never reused during its lifetime.
    ShipId addShip(TeamId team, std::unique_ptr<Ship> ship,
            std::unique_ptr<ShipController> controller);

    // Advances exactly one fixed tick; no wall-clock wait or presentation.
    // nullopt means continue; a value is the completed battle result.
    // After completion, further calls return the same result without advancing.
    // Native exceptions end this instance; caller must discard it, not resume it.
    std::optional<BattleResult> step();
    CombatView observe(ShipId viewer) const; // Reject IDs not in this battle.
    std::optional<ShipSnapshot> ship(ShipId id) const;
    std::optional<ProjectileSnapshot> projectile(ProjectileId id) const;
    Tick tick() const noexcept;

private:
    friend class BattleContext;
    struct ShipEntry {
        ShipId id;
        TeamId team;
        std::unique_ptr<Ship> ship;
        std::unique_ptr<ShipController> controller;
    };
    struct ProjectileEntry {
        ProjectileId id;
        std::unique_ptr<Projectile> projectile;
    };
    struct Storage;
    // RNG, collision state, pending commands, result and tick are hidden here.
    std::unique_ptr<Storage> storage_;
    std::vector<ShipEntry> ships_;
    std::vector<ProjectileEntry> projectiles_;
    // Never mutate active containers from a callback. Deferred queues prevent
    // invalidating iteration and make phase ordering explicit.
    std::vector<ProjectileEntry> pendingProjectiles_;
    std::vector<ProjectileId> pendingDetonations_;
};

// Example of remote-controlled projectile ownership; implementation is future work.
class ChenjesuShip : public Ship {
    // Battle owns the crystal. A failed lookup simply clears this optional.
    std::optional<ProjectileId> activeCrystal_;
    ShipCommands previousCommands_;
    // beginTick requests detonation on a held -> released primary-fire edge.
    // All pure virtual methods remain unimplemented in this declaration sketch.
};
} // namespace uqm::design::combat
#endif
