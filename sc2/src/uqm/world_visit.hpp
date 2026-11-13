#ifndef UQM_WORLD_VISIT_HPP_
#define UQM_WORLD_VISIT_HPP_
#include <memory>
#include "comm.h"
#include "planet_report.hpp"
#include "campaign_encounters.hpp"
struct solarsys_state;
struct star_desc;
struct planet_desc;
struct PlanetGeneratedData;
namespace uqm {
class GameSession;
class StoryState;
class World;
// Synchronous visit controller. Session, system and selected world outlive this
// object and all its children. Owns report assets and the orbit location,
// graphics and presentation scopes. Teardown never publishes gameplay state.
class WorldVisit {
public:
    WorldVisit(GameSession &session, solarsys_state &system, World &world);
    ~WorldVisit();
    WorldVisit(const WorldVisit &) = delete;
    WorldVisit &operator=(const WorldVisit &) = delete;
    // Invoke the constructor-selected script, or ordinary orbit for nullopt.
    // False means an exit/story handoff prevents resuming system flight.
    bool run();
    // Debug enumeration only: caller already owns the selected graphics borrow.
    // Populate script-specific data without modes, audio/UI or location changes.
    static void inspectPreparedWorld(GameSession &session, solarsys_state &system);
    // Ordinary generation followed by an orbital child mode, unless exiting.
    void enterOrbit();
    // Script operations borrow current session state and perform synchronous
    // child-mode handoff. Pending exits prevent further preparation/presentation.
    GameSession &session() const noexcept { return session_; }
    StoryState &story() const noexcept;
    CampaignEncounters &encounters() const noexcept;
    PlanetGeneratedData &data() noexcept;
    // The returned report belongs to this visit and outlives synchronous children.
    PlanetReport &report() noexcept { return report_; }
    const star_desc &star() const;
    COUNT converse(CONVERSATION conversation);
    void prepareOrbit();
    // Consumes an optional surface-definition drawable, including on cancellation.
    void loadSurface(FRAME definition);
    void enterPreparedOrbit();
    bool canInteract() const;
    void reportDiscovery();
    void restoreBorder();
    // Transitional descriptor access for the extracted scripts, not ownership.
    solarsys_state &legacySystem() noexcept { return system_; }
    planet_desc &legacyDescriptor();
private:
    WorldVisit(GameSession &, solarsys_state &, World &, bool inspection);
    bool canContinue() const;
    class Resources;
    GameSession &session_;
    solarsys_state &system_;
    World &world_;
    bool inspection_ = false;
    PlanetReport report_;
    WorldVisit *previousVisit_ = nullptr; // Restored before this visit releases its resources.
    std::unique_ptr<Resources> resources_;
};
}
#endif
