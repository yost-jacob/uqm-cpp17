#include "combat_mode.hpp"
#include "game_state.hpp"
#include "globdata.h"
#include "battle_driver.hpp"
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
        : GameMode(host, seed), driver_(engine()), source_(source), preparation_(std::move(preparation)) {
        if (!preparation_) throw std::invalid_argument("combat requires preparation");
    }
    ModeResult run() override {
        if (ready_) {
            driver_.run();
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
        ready_ = preparation_->prepare(engine());
    }
private:
    CombatDriver driver_;
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
std::unique_ptr<GameMode> createCampaignCombatMode(GameSession &session, RuntimeShipRoster &source) {
    return createCampaignCombatMode(session, source, session.nextModeSeed());
}
std::unique_ptr<GameMode> createCampaignCombatMode(GameSession &session, RuntimeShipRoster &source, std::uint32_t seed) {
    return createCombatMode(session.dispatcher(), source, seed,
                            createCampaignCombatPreparation(session.player().story()));
}
}
