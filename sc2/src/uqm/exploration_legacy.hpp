#ifndef UQM_EXPLORATION_LEGACY_HPP_
#define UQM_EXPLORATION_LEGACY_HPP_
#include "system_flight.hpp"
#include "world.hpp"
#include "player_location.hpp"
namespace uqm {
class PlayerEntity;
// Encode the existing save markers from explicit native state. Borrowed objects
// outlive the call; this changes only the legacy projection in player location.
void prepareSystemSave(SolarSystemLocation &location, const SolarSystem &system,
    const SystemFlightState &flight, const STAMP &ship);
// Narrow scope for legacy save-summary/debug entry points. It stores no
// SOLARSYS_STATE or rendering/analysis environment. There is no current-context
// getter: only the C adapters in its implementation can resolve the binding.
// All referenced owners outlive this synchronous scope, including child menus.
class ExplorationLegacyBinding {
public:
    ExplorationLegacyBinding(PlayerEntity &player, const std::unique_ptr<SolarSystem> &system,
        SystemFlightState &flight, const STAR_DESC &star, const STAMP &ship);
    ~ExplorationLegacyBinding();
    ExplorationLegacyBinding(const ExplorationLegacyBinding &) = delete;
    ExplorationLegacyBinding &operator=(const ExplorationLegacyBinding &) = delete;
    bool inOrbit() const;
    void prepareSave();
    void copyName(UNICODE *buffer, COUNT capacity) const;
    void refreshLimits();
private:
    PlayerEntity &player_;
    const std::unique_ptr<SolarSystem> &system_;
    SystemFlightState &flight_;
    const STAR_DESC &star_;
    const STAMP &ship_;
    ExplorationLegacyBinding *previous_;
};
// Optional debug operation; no active exploration means there are no limits to refresh.
}
extern "C" void RefreshExplorationFlightLimits(void);
#endif
