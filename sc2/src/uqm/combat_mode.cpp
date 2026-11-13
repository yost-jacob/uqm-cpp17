#include "combat_mode.hpp"
#include "game_state.hpp"
#include "globdata.h"
#include "battle_driver.hpp"
#include "libs/inplib.h"
#include <stdexcept>
#include <utility>
namespace uqm {
namespace {
// Shared by campaign encounters and Super Melee. Sessions supply participants,
// seed and scoped preparation; only this mode invokes the combat driver.
class CombatMode final : public GameMode {
public:
    CombatMode(Session &session, RuntimeShipRoster &source, std::uint32_t seed,
               std::unique_ptr<CombatPreparation> preparation)
        : GameMode(session.dispatcher(), seed), session_(session), driver_(engine(), session.activity()), source_(source), preparation_(std::move(preparation)) {
        if (!preparation_) throw std::invalid_argument("combat requires preparation");
    }
    ~CombatMode() override {
        SessionActivation legacy{session_};
        RandomBinding random{engine().random()};
        engine().reset(); // Entity borrows end before preparation assets expire.
        preparation_.reset();
        participants_.reset();
    }
    ModeResult run() override {
        SessionActivation legacy{session_};
        RandomBinding random{engine().random()};
        if (ready_) {
            const auto cancelled = [&] { return QuitPosted || dispatcher().exitRequest().has_value()
                || (session_.activity() & CHECK_LOAD); };
            if (driver_.run(std::exchange(session_.instantVictory(), false), cancelled))
                preparation_->complete(); // Explicit completion only, never cancellation.
        }
        preparation_.reset(); // Restore caller services before choosing its result.
        if (auto exit = dispatcher().exitRequest()) return *exit;
        if (QuitPosted) return SessionExit::shutdown();
        if (session_.activity() & CHECK_ABORT) return SessionExit::title();
        return ModeCompleted{};
    }
protected:
    void initialize() override {
        SessionActivation legacy{session_};
        RandomBinding random{engine().random()};
        participants_.emplace(source_, engine().shipRoster());
        ready_ = preparation_->prepare(engine());
    }
private:
    Session &session_;
    CombatDriver driver_;
    RuntimeShipRoster &source_;
    std::optional<RuntimeRosterTransfer> participants_;
    std::unique_ptr<CombatPreparation> preparation_;
    bool ready_ = false;
};
}
std::unique_ptr<GameMode> createCombatMode(Session &session, RuntimeShipRoster &source,
        std::uint32_t seed, std::unique_ptr<CombatPreparation> preparation) {
    return std::make_unique<CombatMode>(session, source, seed, std::move(preparation));
}
std::unique_ptr<GameMode> createCampaignCombatMode(GameSession &session, RuntimeShipRoster &source) {
    return createCampaignCombatMode(session, source, session.nextModeSeed());
}
std::unique_ptr<GameMode> createCampaignCombatMode(GameSession &session, RuntimeShipRoster &source, std::uint32_t seed) {
    return createCombatMode(session, source, seed,
                            createCampaignCombatPreparation(session));
}
}
