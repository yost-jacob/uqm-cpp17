#ifndef UQM_BATTLE_SCENE_HPP_
#define UQM_BATTLE_SCENE_HPP_
#include "libs/sndlib.h"
#include "globdata.h"
#include "game_mode.hpp"
namespace uqm {
class SimulationEngine;
class ModeSuspension;
class SpaceNavigation;
namespace detail {
// Internal scene/resource owner. initialize runs after construction so partial
// acquisition also unwinds. complete explicitly publishes crew/results; the
// destructor only cancels/releases and never calls gameplay completion hooks.
class BattleScene {
public:
    BattleScene(SimulationEngine &engine, ACTIVITY &activity) noexcept : engine_(engine), activity_(activity) {}
    // navigation and its engine outlive this synchronous scene, including teardown.
    BattleScene(SimulationEngine &engine, ACTIVITY &activity, SpaceNavigation &navigation) noexcept
        : engine_(engine), activity_(activity), navigation_(&navigation) {}
    ~BattleScene();
    BattleScene(const BattleScene &) = delete;
    BattleScene &operator=(const BattleScene &) = delete;
    void initialize();
    void complete();
    void playMusic();
private:
    void loadMusic();
    std::unique_ptr<ModeSuspension> drawing_;
    SimulationEngine &engine_;
    ACTIVITY &activity_;
    SpaceNavigation *navigation_ = nullptr; // Borrowed; null denotes combat.
    MUSIC_REF music_ = 0;
    BattleScene *previous_ = nullptr;
    bool bound_ = false, spaceAcquired_ = false, completed_ = false;
};
}
}
#endif
