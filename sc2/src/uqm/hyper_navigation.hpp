#ifndef UQM_HYPER_NAVIGATION_HPP_
#define UQM_HYPER_NAVIGATION_HPP_
#include "hyper.h"
#include "encount.h"
#include "menustat.h"
#include "player_location.h"
namespace uqm {
class CampaignClock;
class StoryState;
class PlayerEntity;
class GameSession;
class SimulationEngine;
// One navigation visit, owned by HyperSpaceMode. C element/input callbacks borrow
// it through a scoped session binding; all native helpers use this object's state.
class HyperSpaceNavigation {
public:
    // Both borrows outlive this navigation visit and its C callbacks.
    HyperSpaceNavigation(PlayerEntity &player, SimulationEngine &engine, CampaignClock &clock) noexcept;
    void run(GameSession &session);
    void frame();
    HyperSpaceNavigation(const HyperSpaceNavigation &) = delete;
    HyperSpaceNavigation &operator=(const HyperSpaceNavigation &) = delete;
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
    friend class NavigationBinding;
    // The realm selected by this mode must match the player variant.
    PLAYER_TRAVEL_POSITION &travel();
    PlayerEntity &player_;
    CampaignClock &clock_;
    SimulationEngine &engine_;
    StoryState &story_;
    bool starsBound_ = false; // Resource swap lease; release is idempotent, including cancellation.
    BYTE fuel_ticks = 1;
    COUNT hyper_dx = 0, hyper_dy = 0, hyper_extra = 0;
};
// Binding never owns navigation. Session and owner outlive every C callback;
// registration is cleared/restored before the owning mode can be destroyed.
class NavigationBinding {
public:
    NavigationBinding(GameSession &session, HyperSpaceNavigation &navigation);
    ~NavigationBinding();
    NavigationBinding(const NavigationBinding &) = delete;
    NavigationBinding &operator=(const NavigationBinding &) = delete;
private:
    GameSession &session_;
    HyperSpaceNavigation *previous_;
};
} // namespace uqm
#endif
