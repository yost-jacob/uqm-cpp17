#include "combat_mode.hpp"
#include "game_state.hpp"
#include "globdata.h"
#include "libs/inplib.h"
namespace uqm {
namespace {
class CampaignCombatMode final : public GameMode {
public:
    CampaignCombatMode(GameSession &session, std::uint32_t seed)
        : GameMode(session.stateOwner(), session.playerId(), seed), session_(session),
          story_(session.player().story()), source_(simulationEngine().shipRoster()) {}
    ModeResult run() override {
        runCampaignCombat(story_);
        if (auto exit = session_.menuExit()) return *exit;
        if (QuitPosted) return SessionExit::shutdown();
        if (GLOBAL(CurrentActivity) & CHECK_ABORT) return SessionExit::title();
        return ModeCompleted{};
    }
protected:
    void initialize() override { participants_.emplace(source_, engine().shipRoster()); }
private:
    GameSession &session_;
    StoryState &story_;
    RuntimeShipRoster &source_;
    std::optional<RuntimeRosterTransfer> participants_;
};
}
std::unique_ptr<GameMode> createCampaignCombatMode(GameSession &session) {
    return createCampaignCombatMode(session, session.nextModeSeed());
}
std::unique_ptr<GameMode> createCampaignCombatMode(GameSession &session, std::uint32_t seed) {
    return std::make_unique<CampaignCombatMode>(session, seed);
}
}
