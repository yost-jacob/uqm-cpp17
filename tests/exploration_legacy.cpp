#include "uqm/exploration_legacy.hpp"
#include "uqm/game_state.hpp"
#include "uqm/globdata.h"
#include "uqm/gamestr.h"
#include "libs/strlib.h"
#include <boost/test/unit_test.hpp>
#include <stdexcept>
extern "C" { STRING GameStrings = nullptr; } // No UI string table is installed in this fixture.
using namespace uqm;
BOOST_AUTO_TEST_CASE(legacy_save_operations_borrow_only_native_state_and_restore_nested_bindings)
{
    GameSession first{std::make_shared<GameState>(UserProfile{SinglePlayerId, "Save pilot"}), SinglePlayerId};
    GameSession second{std::make_shared<GameState>(UserProfile{SinglePlayerId, "Other pilot"}), SinglePlayerId};
    SessionActivation activation{first};
    auto &location = first.player().enterSolarSystem();
    second.player().enterSolarSystem();
    const WorldId planet{first.world().id(), StarId{3}, 0, {}};
    auto system = std::make_unique<SolarSystem>(StarId{3}, first.worldOwner(),
        std::vector<WorldDefinition>{{planet, WorldKind::Planet, 1, false, "Planet", 2}});
    std::vector<std::unique_ptr<World>> moons;
    for (unsigned i = 0; i < 2; ++i) {
        auto id = planet; id.moon = i;
        moons.push_back(std::make_unique<World>(WorldDefinition{id, WorldKind::Moon, i, false, "Moon"}, first.worldOwner()));
    }
    system->world(0).replaceChildren(std::move(moons));
    system->enterInnerSystem(planet);
    auto otherSystem = std::make_unique<SolarSystem>(StarId{3}, second.worldOwner(), std::vector<WorldDefinition>{});
    SystemFlightState flight, otherFlight;
    flight.outerAnchor = {100, 200}; flight.turnCounter = 3;
    STAMP ship{}; ship.frame = SetAbsFrameIndex(nullptr, 4);
    STAR_DESC star{}; star.Index = 255; // No named-body resource lookup in this fixture.
    BOOST_TEST(!playerInSolarSystem());
    {
        ExplorationLegacyBinding binding{first.player(), system, flight, star, ship};
        BOOST_TEST(playerInSolarSystem());
        BOOST_TEST(!playerInPlanetOrbit());
        SaveSolarSysLocation();
        BOOST_TEST(location.flight.planet == 1u);
        BOOST_TEST(location.flight.facing == 5u);
        BOOST_TEST(location.flight.loadOrbit == 0u);
        BOOST_TEST(location.flight.position.x == 100);
        BOOST_TEST(location.flight.position.y == 200);
        RefreshExplorationFlightLimits();
        BOOST_TEST(flight.maxSpeed > 0u);
        BOOST_TEST(flight.turnCounter == 3u);
        const auto moon = system->world(0).child(1).id();
        system->selectWorld(moon);
        location.enterOrbit(moon);
        SaveSolarSysLocation();
        BOOST_TEST(location.flight.loadOrbit == 3u);
        utf8StringCopy(GLOBAL_SIS(PlanetName), sizeof(GLOBAL_SIS(PlanetName)), "OddPlanet");
        char name[32]{};
        GetPlanetOrMoonName(name, sizeof name);
        BOOST_TEST(std::string(name) == "OddPlanet-B");
        try {
            ExplorationLegacyBinding nested{second.player(), otherSystem, otherFlight, star, ship};
            BOOST_TEST(!playerInPlanetOrbit());
            throw std::runtime_error("cancel nested visit");
        } catch (const std::runtime_error &) {}
        BOOST_TEST(playerInPlanetOrbit());
        SaveSolarSysLocation();
        BOOST_TEST(location.flight.loadOrbit == 3u);
        BOOST_TEST(std::get<SolarSystemLocation>(second.player().location()).flight.loadOrbit == 0u);
    }
    BOOST_TEST(!playerInSolarSystem());
    BOOST_TEST(!playerInPlanetOrbit());
    const auto speed = flight.maxSpeed;
    RefreshExplorationFlightLimits(); // Optional debug request after exploration ended.
    BOOST_TEST(flight.maxSpeed == speed);
}
