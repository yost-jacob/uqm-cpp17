#ifndef UQM_BATTLE_SCENE_HPP_
#define UQM_BATTLE_SCENE_HPP_
#include "libs/sndlib.h"
namespace uqm {
class SimulationEngine;
namespace detail {
enum class SceneKind { Combat, Navigation };
// Internal scene/resource owner. initialize runs after construction so partial
// acquisition also unwinds. complete explicitly publishes crew/results; the
// destructor only cancels/releases and never calls gameplay completion hooks.
class BattleScene {
public:
    BattleScene(SimulationEngine &engine, SceneKind kind) noexcept : engine_(engine), kind_(kind) {}
    ~BattleScene();
    BattleScene(const BattleScene &) = delete;
    BattleScene &operator=(const BattleScene &) = delete;
    void initialize();
    void complete();
    void playMusic();
private:
    void loadMusic();
    SimulationEngine &engine_;
    SceneKind kind_;
    MUSIC_REF music_ = 0;
    BattleScene *previous_ = nullptr;
    bool bound_ = false, spaceAcquired_ = false, completed_ = false;
};
}
}
#endif
