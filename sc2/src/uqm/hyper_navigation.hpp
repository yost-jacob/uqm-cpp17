#ifndef UQM_HYPER_NAVIGATION_HPP_
#define UQM_HYPER_NAVIGATION_HPP_
#include "hyper.h"
#include "navigation_transition.hpp"
#include "navigation_legacy.hpp"
#include "navigation_encounters.hpp"
#include "campaign_encounters.hpp"
#include "encount.h"
#include "menustat.h"
#include "player_location.h"
namespace uqm {
class NavigationPresentation;
class CampaignClock;
class StoryState;
class PlayerEntity;
class GameSession;
class SimulationEngine;
// One navigation visit, owned by HyperSpaceMode or QuasiSpaceMode. Engine-owned
// entities borrow it until scene reset; all native helpers use explicit references.
class SpaceNavigation {
public:
    // session and engine outlive the visit. Its fixed realm must match the
    // player location before run() or a callback; definitions/fleets are loaded
    // by the owning campaign before navigation starts.
    SpaceNavigation(GameSession &session, SimulationEngine &engine, NavigationRealm realm);
    // Borrowed mode-owned engine; must be selected for the synchronous scene.
    SimulationEngine &engine() noexcept { return engine_; }
    std::optional<NavigationTransition> run();
    std::optional<NavigationTransition> transition() const noexcept { return transition_; }
    // Normalize portal collision-side bookkeeping only while still navigating;
    // never overwrite the destination already staged for a successor mode.
    void settlePortalSide(bool defiesPhysics);
    // Inclusive search bounds, in universe coordinates. Returns owned IDs in
    // catalogue order and always uses this mode's realm and owning catalogue.
    std::vector<StarId> starsNear(const POINT &position, SIZE xbounds, SIZE ybounds) const;
    void frame();
    void saveFacing(UWORD facing) { travel().flight.facing = facing; }
    // Stage a portal destination in legacy logical coordinates. The outgoing
    // location variant remains valid; the successor mode converts it on entry.
    void prepareRealmTransition(NavigationRealm destination, PLAYER_MAP_POSITION position);
    NavigationRealm realm() const noexcept { return realm_; }
    bool isQuasiSpace() const noexcept { return realm_ == NavigationRealm::QuasiSpace; }
    SpaceNavigation(const SpaceNavigation &) = delete;
    SpaceNavigation &operator=(const SpaceNavigation &) = delete;
    enum TransitionType { RANDOM_ENCOUNTER_TRANSITION, INTERPLANETARY_TRANSITION, ARILOU_SPACE_TRANSITION };
    // Legacy engine entry points. Inputs retain their existing pointer contracts.
    void MoveSIS(SIZE *pdx, SIZE *pdy);
    void check_hyperspace_encounter(void);
    BOOLEAN LoadHyperspace(void);
    BOOLEAN FreeHyperspace(void);
    void ElementToUniverse(ELEMENT *ElementPtr, POINT *pPt);
    void cleanup_hyperspace(void);
    void InterplanetaryTransition(ELEMENT *ElementPtr);
    void ArilouSpaceTransition(void);
    void unhyper_transition(ELEMENT *ElementPtr);
    void init_transition(ELEMENT *ElementPtr0, ELEMENT *ElementPtr1,
		TransitionType which_transition);
    BOOLEAN hyper_transition(ELEMENT *ElementPtr);
    void hyper_collision(ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1);
    void hyper_death(ELEMENT *ElementPtr);
    void arilou_space_death(ELEMENT *ElementPtr);
    void arilou_space_collision(ELEMENT *ElementPtr0,
		POINT *pPt0, ELEMENT *ElementPtr1, POINT *pPt1);
    HELEMENT AllocHyperElement(const POINT *elem_pt);
    void AddAmbientElement(void);
    void encounter_transition(ELEMENT *ElementPtr);
    void encounter_collision(ELEMENT *ElementPtr0, POINT *pPt0,
		ELEMENT *ElementPtr1, POINT *pPt1);
    HELEMENT AddEncounterElement(ENCOUNTER *EncounterPtr, POINT *puniverse);
    void DrawHyperGrid(COORD ux, COORD uy, COORD ox, COORD oy);
    bool ProcessEncounter(ENCOUNTER *EncounterPtr, POINT *puniverse,
		COORD ox, COORD oy, STAMP *stamp);
    void ProcessEncounters(POINT *puniverse, COORD ox, COORD oy);
    void SeedUniverse(void);
    BOOLEAN DoHyperspaceMenu(MENU_STATE *pMS);
    void HyperspaceMenu(void);
private:
    // The realm selected by this mode must match the player variant.
    PLAYER_TRAVEL_POSITION &travel();
    const StarMap &catalogue() const;
    std::optional<StarId> &selectedStar() noexcept;
    detail::NavigationLegacyView legacy_;
    std::optional<NavigationTransition> transition_;
    GameSession &session_;
    CampaignEncounters &records_;
    NavigationEncounterViews encounters_;
    const NavigationRealm realm_; // Fixed for this mode; portal crossings select a successor.
    PlayerEntity &player_;
    CampaignClock &clock_;
    SimulationEngine &engine_;
    StoryState &story_;
    NavigationPresentation *presentation_ = nullptr; // Borrowed only during synchronous run().
    BYTE fuel_ticks = 1;
    COUNT hyper_dx = 0, hyper_dy = 0, hyper_extra = 0;
};
// element must be the live flagship in the selected navigation engine. The
// returned borrow ends when its engine slot is released/reset; mismatch throws.
SpaceNavigation &navigationFor(ELEMENT &element);
} // namespace uqm
#endif
