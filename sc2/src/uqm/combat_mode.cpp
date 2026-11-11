#include "combat_mode.hpp"
#include "game_state.hpp"
#include "globdata.h"
#include "battle.h"
#include "libs/inplib.h"
#include <stdexcept>
namespace uqm {
namespace {
// Shared by campaign encounters and Super Melee. Sessions supply participants,
// seed and scoped preparation; only this mode invokes the combat driver.
class CombatMode final : public GameMode {
public:
    CombatMode(ModeDispatcher &host, RuntimeShipRoster &source, std::uint32_t seed,
               std::unique_ptr<CombatPreparation> preparation)
        : GameMode(host, seed), source_(source), preparation_(std::move(preparation)) {
        if (!preparation_) throw std::invalid_argument("combat requires preparation");
    }
    ModeResult run() override {
        if (ready_) {
            Battle(nullptr, nullptr);
            preparation_->complete(); // Publish gameplay results explicitly.
        }
        preparation_.reset(); // Restore caller services before choosing its result.
        if (auto exit = dispatcher().menuExit()) return *exit;
        if (QuitPosted) return SessionExit::shutdown();
        if (GLOBAL(CurrentActivity) & CHECK_ABORT) return SessionExit::title();
        return ModeCompleted{};
    }
protected:
    void initialize() override {
        participants_.emplace(source_, engine().shipRoster());
        ready_ = preparation_->prepare();
    }
private:
    RuntimeShipRoster &source_;
    std::optional<RuntimeRosterTransfer> participants_;
    std::unique_ptr<CombatPreparation> preparation_;
    bool ready_ = false;
};
}
std::unique_ptr<GameMode> createCombatMode(ModeDispatcher &host, RuntimeShipRoster &source,
        std::uint32_t seed, std::unique_ptr<CombatPreparation> preparation) {
    return std::make_unique<CombatMode>(host, source, seed, std::move(preparation));
}
std::unique_ptr<GameMode> createCampaignCombatMode(GameSession &session) {
    return createCampaignCombatMode(session, session.nextModeSeed());
}
std::unique_ptr<GameMode> createCampaignCombatMode(GameSession &session, std::uint32_t seed) {
    return createCombatMode(session.dispatcher(), simulationEngine().shipRoster(), seed,
                            createCampaignCombatPreparation(session.player().story()));
}
}
