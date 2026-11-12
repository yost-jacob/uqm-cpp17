#ifndef UQM_BATTLE_SCENE_HPP_
#define UQM_BATTLE_SCENE_HPP_
#include "libs/sndlib.h"
namespace uqm {
class SimulationEngine;
class SpaceNavigation;
namespace detail {
// Internal scene/resource owner. initialize runs after construction so partial
// acquisition also unwinds. complete explicitly publishes crew/results; the
// destructor only cancels/releases and never calls gameplay completion hooks.
class BattleScene {
public:
    explicit BattleScene(SimulationEngine &engine) noexcept : engine_(engine) {}
    // navigation and its engine outlive this synchronous scene, including teardown.
    BattleScene(SimulationEngine &engine, SpaceNavigation &navigation) noexcept
        : engine_(engine), navigation_(&navigation) {}
    ~BattleScene();
    BattleScene(const BattleScene &) = delete;
    BattleScene &operator=(const BattleScene &) = delete;
    void initialize();
    void complete();
    void playMusic();
private:
    void loadMusic();
    SimulationEngine &engine_;
    SpaceNavigation *navigation_ = nullptr; // Borrowed; null denotes combat.
    MUSIC_REF music_ = 0;
    BattleScene *previous_ = nullptr;
    bool bound_ = false, spaceAcquired_ = false, completed_ = false;
};
}
}
#endif
