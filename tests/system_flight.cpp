#include "uqm/system_runtime.hpp"
#include "uqm/combat_entity_legacy.hpp"
#include "uqm/simulation_engine_legacy.hpp"
#include "uqm/grpinfo.h"
#include "uqm/ipdisp.h"
#include "uqm/globdata.h"
#include "uqm/encount.h"
#include <boost/test/unit_test.hpp>
// Rendering/collision setup is outside these tests. Real engine allocation,
// native entities, queues, velocity and flight logic are linked.
extern "C" void SetUpElement(ELEMENT *element) { element->next = element->current; }
extern "C" void RemoveElement(HLINK link) { RemoveQueue(&disp_q, link); }
using namespace uqm;
BOOST_AUTO_TEST_CASE(system_flight_limits_reconfigure_without_resetting_runtime_counters)
{
    SystemFlightState first, second;
    SISLoadout loadout;
    loadout.drives.fill(EMPTY_SLOT);
    loadout.jets.fill(EMPTY_SLOT);
    first.configure(loadout);
    BOOST_TEST(first.maxSpeed == 5 * IP_SHIP_THRUST_INCREMENT);
    BOOST_TEST(first.turnWait == IP_SHIP_TURN_WAIT);
    first.turnCounter = 3; first.thrustCounter = 2;
    first.waitIntersect = static_cast<COUNT>(~0u); first.active = true;
    loadout.drives.fill(FUSION_THRUSTER); loadout.jets.fill(TURNING_JETS);
    first.configure(loadout);
    BOOST_TEST(first.maxSpeed == (5 + NUM_DRIVE_SLOTS) * IP_SHIP_THRUST_INCREMENT);
    BOOST_TEST(first.turnWait == IP_SHIP_TURN_WAIT - NUM_JET_SLOTS * IP_SHIP_TURN_DECREMENT);
    BOOST_TEST(first.turnCounter == 3u);
    BOOST_TEST(first.thrustCounter == 2u);
    BOOST_TEST(first.waitIntersect == static_cast<COUNT>(~0u));
    BOOST_TEST(first.active);
    BOOST_TEST(!second.active);
    BOOST_TEST(second.waitIntersect == 0u);
}
BOOST_AUTO_TEST_CASE(flight_entities_borrow_their_player_and_encounter_state_not_the_active_session)
{
    GameSession first{std::make_shared<GameState>(UserProfile{SinglePlayerId, "First"}), SinglePlayerId};
    GameSession second{std::make_shared<GameState>(UserProfile{SinglePlayerId, "Second"}), SinglePlayerId};
    SessionActivation activation{first};
    first.player().enterSolarSystem(); second.player().enterSolarSystem();
    SimulationEngine simulation{4};
    SimulationBinding binding{simulation};
    UQM_SYSTEM_RUNTIME runtime{first.player().story(), simulation, first.worldOwner(), nullptr, &first};
    auto &system = runtime.engine;
    system.worlds = &runtime;
    system.SunDesc[0].radius = MAX_ZOOM_RADIUS;
    system.SunDesc[0].NumPlanets = 1;
    system.PlanetDesc[0].image.origin = {static_cast<COORD>(SIS_SCREEN_WIDTH / 2), static_cast<COORD>(SIS_SCREEN_HEIGHT / 2)};
    const SystemGeometry geometry{system.SunDesc[0], system.PlanetDesc};
    BOOST_TEST(geometry.planetLocation(0).x == 0);
    BOOST_CHECK_THROW(geometry.planetLocation(1), std::out_of_range);
    runtime.system = std::make_unique<SolarSystem>(StarId{0}, first.worldOwner(), std::vector<WorldDefinition>{});
    InitQueue(&GLOBAL(ip_group_q), 1, QUEUE_IP_GROUP);
    struct ClearGroups { QUEUE &queue; ~ClearGroups() { UninitQueue(&queue); } } clear{GLOBAL(ip_group_q)};
    const auto link = AllocLink(&GLOBAL(ip_group_q));
    BOOST_REQUIRE(link);
    auto &group = *static_cast<IP_GROUP *>(link);
    group.race_id = 2; group.in_system = 1; group.dest_loc = 1; group.task = ON_STATION | IGNORE_FLAGSHIP;
    PutQueue(&GLOBAL(ip_group_q), link);
    first.player().story().set(StoryCounter::EscapeCounter, 3);
    second.player().story().set(StoryCounter::EscapeCounter, 7);
    EncounterRace = -1;
    GLOBAL(CurrentActivity) = 0;
    auto &firstActivity = GLOBAL(CurrentActivity);
    ZeroVelocityComponents(&GLOBAL(velocity));
    runSystemMissions(system);
    BOOST_TEST(simulation.size() == 2u);
    auto *flagship = static_cast<ELEMENT *>(GetHeadElement());
    auto *npc = static_cast<ELEMENT *>(GetTailElement());
    auto *entity = detail::borrowEntity(disp_q, flagship);
    auto *npcEntity = detail::borrowEntity(disp_q, npc);
    BOOST_REQUIRE(entity); BOOST_REQUIRE(npcEntity);
    {
        SessionActivation other{second};
        entity->update(*flagship);
        BOOST_TEST(first.player().story().read(StoryCounter::EscapeCounter) == 2u);
        BOOST_TEST(second.player().story().read(StoryCounter::EscapeCounter) == 7u);
        const auto oldLife = npc->life_span;
        npcEntity->expire(*npc);
        BOOST_TEST(npc->life_span == oldLife + 1); // Preserve the legacy death-hook movement stage.
        first.player().story().set(StoryCounter::EscapeCounter, 0);
        POINT point{};
        // The existing two-phase collision policy starts the ordinary encounter.
        flagship->state_flags |= COLLISION;
        npcEntity->collide(*npc, point, *flagship, point);
        BOOST_TEST(group.task & REFORM_GROUP);
        BOOST_TEST(firstActivity & START_ENCOUNTER);
        BOOST_TEST(!(GLOBAL(CurrentActivity) & START_ENCOUNTER));
    }
    simulation.reset(); // End entity borrows before clearing the group queue.
}
