#include "battle.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace uqm::design::combat {
namespace {
bool finite(Vector2 value) { return std::isfinite(value.x) && std::isfinite(value.y); }
}
std::vector<std::unique_ptr<Projectile>> ProjectileBehavior::detonate(const ProjectileState &) {
    return {};
}
Projectile::Projectile(ProjectileState state, std::unique_ptr<ProjectileBehavior> behavior)
    : state_(state), behavior_(std::move(behavior)) {
    if (!behavior_) throw std::invalid_argument("projectile requires a behavior");
    if (!finite(state.position) || !finite(state.velocity))
        throw std::invalid_argument("projectile motion must be finite");
    if (state.remainingTicks && *state.remainingTicks == 0)
        throw std::invalid_argument("projectile lifetime must be positive or unbounded");
}
Projectile::~Projectile() = default;
std::vector<std::unique_ptr<Projectile>> Projectile::step(const CombatView &view) {
    if (!active_) return {};
    switch (behavior_->update(state_, view)) {
        case ProjectileAction::Detonate: return detonate();
        case ProjectileAction::Remove: remove(); return {};
        case ProjectileAction::Continue: break;
        default: throw std::logic_error("invalid projectile action");
    }
    state_.position.x += state_.velocity.x;
    state_.position.y += state_.velocity.y;
    if (!finite(state_.position) || !finite(state_.velocity))
        throw std::overflow_error("projectile motion became nonfinite");
    if (state_.remainingTicks) {
        if (*state_.remainingTicks == 0)
            throw std::logic_error("behavior supplied an expired lifetime");
        if (--*state_.remainingTicks == 0) remove();
    }
    return {};
}
std::vector<std::unique_ptr<Projectile>> Projectile::detonate() {
    if (!active_) return {};
    active_ = false; // Repeated requests never spawn a second set of fragments.
    auto behavior = std::move(behavior_);
    return behavior->detonate(state_); // Source ownership ends when this returns.
}
void Projectile::remove() noexcept {
    active_ = false;
    behavior_.reset(); // No detonation effects on collision removal or teardown.
}
void Projectile::retireSource(ShipId source) noexcept {
    if constexpr (!BattleRules::persistentProjectiles) {
        if (state_.source == source) remove();
    }
}
ProjectileAction StraightFlight::update(ProjectileState &, const CombatView &) {
    return ProjectileAction::Continue;
}
SeekingFlight::SeekingFlight(ShipId target, double maxTurnRadians)
    : target_(target), maxTurn_(maxTurnRadians) {
    constexpr double pi = 3.14159265358979323846;
    if (!std::isfinite(maxTurnRadians) || maxTurnRadians < 0 || maxTurnRadians > pi)
        throw std::invalid_argument("invalid seeking turn limit");
}
ProjectileAction SeekingFlight::update(ProjectileState &state, const CombatView &view) {
    if (target_.battle != state.source.battle)
        throw std::invalid_argument("projectile target belongs to another battle");
    const auto target = std::find_if(view.ships.begin(), view.ships.end(),
        [this](const ShipSnapshot &ship) { return ship.id == target_ && ship.crew != 0; });
    if (target == view.ships.end()) return ProjectileAction::Continue;
    const auto speed = std::hypot(state.velocity.x, state.velocity.y);
    const Vector2 delta{target->position.x - state.position.x, target->position.y - state.position.y};
    if (speed == 0 || (delta.x == 0 && delta.y == 0)) return ProjectileAction::Continue;
    constexpr double fullCircle = 6.28318530717958647692;
    const auto current = std::atan2(state.velocity.y, state.velocity.x);
    const auto desired = std::atan2(delta.y, delta.x);
    const auto turn = std::clamp(std::remainder(desired - current, fullCircle), -maxTurn_, maxTurn_);
    const auto facing = current + turn;
    state.velocity = {speed * std::cos(facing), speed * std::sin(facing)};
    return ProjectileAction::Continue;
}
ChenjesuCrystalBehavior::ChenjesuCrystalBehavior(std::shared_ptr<const Ship> source, CrystalFragments fragments)
    : source_(std::move(source)), fragments_(fragments) {
    if (!source_) throw std::invalid_argument("crystal requires its source ship");
    if (fragments.count == 0 || fragments.lifetime == 0
            || !std::isfinite(fragments.speed) || fragments.speed < 0)
        throw std::invalid_argument("invalid crystal fragment configuration");
}
ProjectileAction ChenjesuCrystalBehavior::update(ProjectileState &, const CombatView &) {
    if (source_.use_count() > 2)
        throw std::logic_error("crystal source has an unexpected strong owner");
    if (source_.use_count() == 1 || !source_->commands().firePrimary)
        return ProjectileAction::Detonate;
    return ProjectileAction::Continue;
}
std::vector<std::unique_ptr<Projectile>> ChenjesuCrystalBehavior::detonate(const ProjectileState &state) {
    constexpr double fullCircle = 6.28318530717958647692;
    std::vector<std::unique_ptr<Projectile>> fragments;
    fragments.reserve(fragments_.count);
    for (unsigned i = 0; i < fragments_.count; ++i) {
        const auto angle = fullCircle * i / fragments_.count;
        ProjectileState fragment{state.source, state.team, state.position,
            {fragments_.speed * std::cos(angle), fragments_.speed * std::sin(angle)},
            fragments_.damage, fragments_.lifetime};
        fragments.push_back(std::make_unique<Projectile>(fragment, std::make_unique<StraightFlight>()));
    }
    return fragments;
}
} // namespace uqm::design::combat
