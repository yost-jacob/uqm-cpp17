#include "combat_mode.hpp"
#include "game_state.hpp"
#include "globdata.h"
#include "libs/inplib.h"
namespace uqm {
namespace {
class CampaignCombatMode final : public GameMode {
public:
    explicit CampaignCombatMode(GameSession &session)
        : GameMode(session.stateOwner(), session.playerId()), session_(session),
          story_(session.player().story()) {}
    ModeResult run() override {
        runCampaignCombat(story_);
        if (auto exit = session_.menuExit()) return *exit;
        if (QuitPosted) return SessionExit::shutdown();
        if (GLOBAL(CurrentActivity) & CHECK_ABORT) return SessionExit::title();
        return ModeCompleted{};
    }
private:
    GameSession &session_;
    StoryState &story_;
};
}
std::unique_ptr<GameMode> createCampaignCombatMode(GameSession &session) {
    return std::make_unique<CampaignCombatMode>(session);
}
}
