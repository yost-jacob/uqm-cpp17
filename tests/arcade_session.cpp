#include "uqm/arcade_session.hpp"
#include "uqm/combat_entity_legacy.hpp"
#include "uqm/legacy_runtime.hpp"
#include "uqm/game_state.hpp"
#include "uqm/fleet_legacy.hpp"
#include "uqm/simulation_engine_legacy.hpp"
#include "uqm/state.h"
#include "libs/inplib.h"
#include <boost/test/unit_test.hpp>
#include <functional>
#include <stdexcept>
namespace { std::function<void()> frontend; }
namespace {
class FleetSelection final : public uqm::GameMode {
public:
    explicit FleetSelection(uqm::ArcadeSession &session)
        : GameMode(session.dispatcher(), session.nextModeSeed()) {}
    uqm::ModeResult run() override {
        if (frontend) frontend();
        return QuitPosted ? uqm::ModeResult{uqm::SessionExit::shutdown()}
                          : uqm::ModeResult{uqm::ModeCompleted{}};
    }
};
}
std::unique_ptr<uqm::GameMode> uqm::detail::createArcadeFleetSelectionMode(ArcadeSession &session) {
    return std::make_unique<FleetSelection>(session);
}

BOOST_AUTO_TEST_CASE(arcade_uses_isolated_buffers_and_returns_to_title_without_modifying_campaign)
{
    auto &session = uqm::gameSession();
    auto &story = session.player().story();
    const auto oldDiscoveries = story.discoverySnapshot();
    const auto oldFlag = story.read(uqm::StoryFlag::StarbaseAccessGranted);
    const auto oldCrew = session.player().sis().crew();
    struct Restore {
        uqm::StoryState &story; uqm::SIS &sis;
        uqm::DiscoverySnapshot discoveries; bool flag; COUNT crew;
        ~Restore() { story.restoreDiscoveries(std::move(discoveries));
            story.set(uqm::StoryFlag::StarbaseAccessGranted, flag); sis.setCrew(crew); frontend = {}; }
    } restore{story, session.player().sis(), oldDiscoveries, oldFlag, oldCrew};
    story.set(uqm::StoryFlag::StarbaseAccessGranted, true);
    session.player().sis().setCrew(23);
    const uqm::WorldId world{session.world().id(), StarId{0}, 0, std::nullopt};
    story.visitDiscovery(world, 0);
    auto *campaignData = GetLegacyGlobData();
    auto *campaignSIS = GetPlayerSISState();
    auto *campaignFleet = GetPlayerFleetQueue();
    auto *previousEngine = &uqm::simulationEngine();
    const auto previousRandom = TFB_GetRandomSeed();
    const auto expectedDiscoveries = story.discovery(world).visitedSites;
    frontend = [&] {
        BOOST_TEST(GetLegacyGlobData() != campaignData);
        BOOST_TEST(GetPlayerSISState() != campaignSIS);
        BOOST_TEST(GetPlayerFleetQueue() != campaignFleet);
        BOOST_TEST(&uqm::simulationEngine() != previousEngine);
        BOOST_TEST(TFB_GetRandomSeed() == std::minstd_rand0(71)());
        TFB_Random();
        *GetLegacyGlobData() = {}; *GetPlayerSISState() = {};
        ResetPersonalDiscoveries(); // The real C adapter must leave campaign discoveries alone.
        GLOBAL(CurrentActivity) = SUPER_MELEE | CHECK_ABORT; // Back to title, not application quit.
        uqm::ArcadeSession nested(12);
        BOOST_TEST(static_cast<int>(nested.run().kind) == static_cast<int>(uqm::SessionExitKind::Failed));
    };
    uqm::ArcadeSession arcade(71);
    BOOST_TEST(static_cast<int>(arcade.run().kind) == static_cast<int>(uqm::SessionExitKind::ReturnToTitle));
    BOOST_TEST(GetLegacyGlobData() == campaignData);
    BOOST_TEST(GetPlayerSISState() == campaignSIS);
    BOOST_TEST(GetPlayerFleetQueue() == campaignFleet);
    BOOST_TEST(&uqm::simulationEngine() == previousEngine);
    BOOST_TEST(TFB_GetRandomSeed() == previousRandom);
    BOOST_TEST(story.read(uqm::StoryFlag::StarbaseAccessGranted));
    BOOST_TEST(session.player().sis().crew() == 23u);
    BOOST_CHECK(story.discovery(world).visitedSites == expectedDiscoveries);
}

BOOST_AUTO_TEST_CASE(arcade_failure_destroys_simulation_before_restoring_campaign_binding)
{
    auto *original = GetLegacyGlobData();
    auto *engine = &uqm::simulationEngine();
    unsigned destroyed = 0;
    class Entity final : public uqm::combat::CombatEntity {
    public:
        Entity(unsigned &count, GLOBDATA *data) : count_(count), data_(data) {}
        ~Entity() override { BOOST_TEST(GetLegacyGlobData() == data_); ++count_; }
        void update(ELEMENT &) override {}
    private: unsigned &count_; GLOBDATA *data_;
    };
    frontend = [&] {
        auto &queue = uqm::detail::SimulationAccess::queue(uqm::simulationEngine());
        auto slot = AllocLink(&queue);
        auto &element = *static_cast<ELEMENT *>(slot); element = {};
        uqm::detail::bindEntity(queue, slot, std::make_unique<Entity>(destroyed, GetLegacyGlobData()));
        PutQueue(&queue, slot);
        throw std::runtime_error("frontend failed");
    };
    uqm::ArcadeSession arcade(99);
    const auto exit = arcade.run();
    frontend = {};
    BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::SessionExitKind::Failed));
    BOOST_REQUIRE(exit.error);
    BOOST_CHECK_EXCEPTION(std::rethrow_exception(exit.error), std::runtime_error,
        [](const auto &error) { return std::string(error.what()) == "frontend failed"; });
    BOOST_TEST(destroyed == 1u);
    BOOST_TEST(GetLegacyGlobData() == original);
    BOOST_TEST(&uqm::simulationEngine() == engine);
}

BOOST_AUTO_TEST_CASE(arcade_rejects_live_campaign_and_distinguishes_application_quit)
{
    auto &campaign = uqm::gameSession();
    frontend = [] { BOOST_FAIL("arcade frontend entered beneath campaign"); };
    struct Parent final : uqm::GameMode {
        explicit Parent(uqm::GameSession &session) : GameMode(session.dispatcher(), session.nextModeSeed()) {}
        uqm::ModeResult run() override {
            auto *before = GetLegacyGlobData();
            uqm::ArcadeSession arcade(19);
            BOOST_TEST(static_cast<int>(arcade.run().kind) == static_cast<int>(uqm::SessionExitKind::Failed));
            BOOST_TEST(GetLegacyGlobData() == before);
            return uqm::ModeCompleted{};
        }
    };
    campaign.run(std::make_unique<Parent>(campaign));
    struct Restore { decltype(QuitPosted) value = QuitPosted; ~Restore() { QuitPosted = value; frontend = {}; } } restore;
    frontend = [] { QuitPosted = TRUE; };
    uqm::ArcadeSession arcade(21);
    BOOST_TEST(static_cast<int>(arcade.run().kind) == static_cast<int>(uqm::SessionExitKind::Shutdown));
}

BOOST_AUTO_TEST_CASE(arcade_rejects_campaign_load_after_modes_unwind)
{
    auto *original = GetLegacyGlobData();
    uqm::ArcadeSession arcade(17);
    frontend = [&] { arcade.dispatcher().requestMenuExit(uqm::SessionExit::load(0)); };
    const auto exit = arcade.run();
    frontend = {};
    BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::SessionExitKind::Failed));
    BOOST_REQUIRE(exit.error);
    BOOST_TEST(GetLegacyGlobData() == original);
    BOOST_TEST(!arcade.dispatcher().isRunning());
}
