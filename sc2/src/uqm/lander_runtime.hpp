#ifndef UQM_LANDER_RUNTIME_HPP_
#define UQM_LANDER_RUNTIME_HPP_
#include "system_runtime.hpp"
#include "planets/lander.h"
#include "game_assets.hpp"
#include "encounter_legacy.hpp"
#include <array>
namespace uqm {
inline constexpr unsigned LanderCrewColumns = 6, LanderCrewRows = 2;
// Immutable cached assets, with expedition-local frame selection. No entity may
// retain a lander frame after this lease ends; persistent node art belongs to World.
class LanderAssets {
public:
    explicit LanderAssets(AssetCache &cache);
    std::array<FRAME, 7> frames{};
    SOUND sounds() const noexcept { return sounds_->sound(); }
private:
    std::array<std::shared_ptr<const GraphicAsset>, 7> graphics_;
    std::shared_ptr<const SoundAsset> sounds_;
};
enum class LanderOutcome { Returned, Lost, Cancelled };
// One launch, constructed only when its child mode initializes. The session,
// system/world, orbital visit and engine outlive it. Destruction detaches the
// surface location only; it never returns cargo/crew or charges for a lost lander.
class LanderVisit {
    class SurfaceScope {
    public:
        SurfaceScope(SolarSystemLocation &location, POINT point) : location_(location) { location_.land(point); }
        ~SurfaceScope() { location_.leaveSurface(); }
        POINT &position() noexcept { return location_.surface.value().position; }
    private:
        SolarSystemLocation &location_;
    } surface_;
public:
    LanderVisit(GameSession &owner, SOLARSYS_STATE &system, SimulationEngine &engine, POINT landing);
    LanderVisit(const LanderVisit &) = delete;
    LanderVisit &operator=(const LanderVisit &) = delete;
    // Transfer one crew member before its boarding animation; false when empty.
    // Crew already aboard is never silently refunded by cancellation/destruction.
    bool embarkCrew();
    // Explicit, at most once. Abort/load/shutdown awards nothing and charges no
    // lander loss. Normal return transfers the complete package atomically.
    LanderOutcome finish(bool shutdown = false);
    COUNT freeCargo() const;
    ACTIVITY &activity() const noexcept { return campaign_.activity; }
    GameSession &session;
    SOLARSYS_STATE &system;
    StoryState &story;
    SimulationEngine &simulation;
    LanderAssets assets;
    VELOCITY_DESC velocity{}; // Expedition motion, never the campaign flagship's velocity.
    POINT &curLanderLoc;
    bool InTransit{};
    COUNT ElementLevel{}, MaxElementLevel{}, BiologicalLevel{};
    COUNT ElementAmounts[NUM_ELEMENT_CATEGORIES]{};
    COUNT NumFrames{};
    UNICODE AmountBuf[40]{};
    TEXT MineralText[3]{};
    Color ColorCycle[NUM_TEXT_FRAMES >> 1]{};
    BYTE TectonicsChance{}, WeatherChance{}, FireChance{};
    BYTE lander_flags{};
    int crew_left{}, shieldHit{}, damage_index{}, explosion_index{}, turn_wait{}, weapon_wait{};
private:
    detail::EncounterLegacyView campaign_;
    bool finished_ = false;
};
// Presentation/input kernel. Called only while this visit's engine is selected.
void runLander(LanderVisit &visit);
// Remove launch-only hazards/projectiles without invoking gameplay death hooks.
// The explicit engine keeps persistent surface nodes for the orbital handback.
void removeLanderEffects(SimulationEngine &engine) noexcept;
}
#endif
