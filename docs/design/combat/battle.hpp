#ifndef UQM_DESIGN_COMBAT_BATTLE_HPP_
#define UQM_DESIGN_COMBAT_BATTLE_HPP_

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

// Declaration-only design sketch. Not linked into gameplay.
namespace uqm::design::combat {
using Tick = std::uint64_t;
struct ShipId {
    std::uint64_t battle, serial;
    constexpr bool operator==(ShipId other) const noexcept {
        return battle == other.battle && serial == other.serial;
    }
};
struct ProjectileId { std::uint64_t battle, serial; };
struct TeamId { unsigned value; };
struct Vector2 { double x = 0, y = 0; };
struct ShipCommands {
    bool turnLeft = false, turnRight = false, thrust = false;
    bool firePrimary = false, useSpecial = false;
};
struct ShipDefinition; // Validated JSON properties; flagship loadout resolved.
struct ShipAssets;     // Separate RAII asset owner; absent in headless simulation.
// These are semantic states, not replacements for legacy activity bits yet.
// Battle retains its strong owner through recovery/explosion decisions.
enum class ShipLifecycle { Active, Exploding, Reincarnating, Withdrawing, Retired };
struct ShipSnapshot {
    ShipId id;
    TeamId team;
    Vector2 position, velocity;
    double facing;
    unsigned crew, energy;
    ShipLifecycle lifecycle = ShipLifecycle::Active;
    // Capability and cooldown observations must be added before AI migration.
};
struct ProjectileSnapshot {
    ProjectileId id;
    ShipId source;
    TeamId team;
    Vector2 position, velocity;
    std::optional<unsigned> remainingTicks; // nullopt means behavior-controlled lifetime.
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
// destroyed reports combat loss; it does not authorize final retirement or
// source-owner release (a Pkunk may still reincarnate).
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
class Ship;
class Projectile;
class ProjectileBehavior;

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
    // Only a behavior requiring source lifetime takes ownership. Observers and
    // controllers use copied snapshots. Caller must move its temporary into the
    // behavior before projectile updates, preserving the crystal owner contract.
    // Returns null for unknown/retired IDs; never resurrects a retired source.
    std::shared_ptr<const Ship> retainSource(ShipId id);

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
    Ship() = default;
    virtual ~Ship() = default;
    Ship(const Ship &) = delete;
    Ship &operator=(const Ship &) = delete;
    // Battle publishes every controller's commands before any ship/projectile
    // updates. Reading input never takes another strong ship owner.
    const ShipCommands &commands() const noexcept { return commands_; }
    // Battle calls this for all combatants before executing behavior callbacks.
    void acceptCommands(ShipCommands commands) noexcept { commands_ = commands; }
    // Definition is immutable and remains valid for this instance's lifetime.
    virtual const ShipDefinition &definition() const noexcept = 0;
    virtual ShipSnapshot snapshot() const = 0;
    // Exactly once before common movement/collisions. Commands are held states;
    // retain prior commands inside the instance when release detection is needed.
    virtual void beginTick(BattleContext &context, const ShipCommands &commands) = 0;
    // Once after collision resolution while attached, including any transition
    // processing needed by an exploding/reincarnating ship.
    virtual void endTick(BattleContext &context) = 0;
    // Apply resolved contact damage and defenses; never mutate campaign state.
    virtual DamageResult receiveHit(BattleContext &context, const Hit &hit) = 0;
private:
    friend class Battle;
    ShipCommands commands_;
};

// Concrete projectile data; behavior supplies variation without a second
// projectile inheritance hierarchy. IDs are assigned by the Battle adapter.
struct ProjectileState {
    ShipId source;
    TeamId team;
    Vector2 position, velocity;
    unsigned damage;
    std::optional<unsigned> remainingTicks;
};
enum class ProjectileAction { Continue, Detonate, Remove };
class ProjectileBehavior {
public:
    virtual ~ProjectileBehavior() = default;
    // Mutate guidance/state only. view is a copied tick observation and must
    // not be retained. Never remove objects from the battle during iteration.
    virtual ProjectileAction update(ProjectileState &state, const CombatView &view) = 0;
    // Called once on detonation, never from the destructor. Returned children
    // become Battle-owned after the current iteration; no parent owns them.
    virtual std::vector<std::unique_ptr<Projectile>> detonate(const ProjectileState &state);
};
class Projectile {
public:
    // Reject null behavior and invalid state. No source owner is implicit here.
    Projectile(ProjectileState state, std::unique_ptr<ProjectileBehavior> behavior);
    ~Projectile();
    Projectile(const Projectile &) = delete;
    Projectile &operator=(const Projectile &) = delete;
    const ProjectileState &state() const noexcept { return state_; }
    bool active() const noexcept { return active_; }
    // Executable ownership/motion prototype only: no legacy collision pipeline.
    // Guidance precedes motion; a detonation does not advance the parent first.
    // Children are returned to the caller, not recursively stepped this tick.
    std::vector<std::unique_ptr<Projectile>> step(const CombatView &view);
    std::vector<std::unique_ptr<Projectile>> detonate();
    void remove() noexcept; // Collision/teardown without detonation effects.
private:
    ProjectileState state_;
    std::unique_ptr<ProjectileBehavior> behavior_;
    bool active_ = true;
};

// Source-independent ordinary flight. Motion/lifetime live in Projectile.
class StraightFlight final : public ProjectileBehavior {
public:
    ProjectileAction update(ProjectileState &, const CombatView &) override;
};
struct CrystalFragments {
    unsigned count = 8;
    unsigned lifetime = 10;
    unsigned damage = 2;
    double speed = 1; // Prototype units; legacy conversion is not yet bound.
};
// Example bounded steering toward a copied target observation. No source owner.
// This demonstrates the dependency boundary; legacy missile acceleration,
// target acquisition and arena wrapping have not been ported here.
class SeekingFlight final : public ProjectileBehavior {
public:
    // maxTurnRadians is finite, in [0, pi], per tick. Target must belong to the
    // projectile's battle. Missing target or zero speed preserves current motion.
    SeekingFlight(ShipId target, double maxTurnRadians);
    ProjectileAction update(ProjectileState &state, const CombatView &view) override;
private:
    ShipId target_;
    double maxTurn_;
};
class ChenjesuCrystalBehavior final : public ProjectileBehavior {
public:
    // Move the source into the behavior. During update the only strong owners
    // must be Battle and this crystal. After Battle releases it, count == 1.
    // This local contract is single-threaded and permits only one active crystal.
    // No asset owner, controller, fragment or history may retain another copy.
    ChenjesuCrystalBehavior(std::shared_ptr<const Ship> source, CrystalFragments fragments);
    ProjectileAction update(ProjectileState &, const CombatView &) override;
    std::vector<std::unique_ptr<Projectile>> detonate(const ProjectileState &state) override;
private:
    std::shared_ptr<const Ship> source_;
    CrystalFragments fragments_;
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
    // Accept unique ownership at entry, then retain a shared_ptr internally.
    // This prevents callers from accidentally retaining an extra strong owner.
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
        std::shared_ptr<Ship> ship;
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

// The Chenjesu ship selects ChenjesuCrystalBehavior on firing. The behavior
// reads source_->commands(), so no crystal ID or release edge is needed in Ship.
// The source is kept allocated until the crystal detonates; Battle releases its
// strong owner at the audited ship-retirement boundary, not at arbitrary UI time.
} // namespace uqm::design::combat
#endif
