#include "encounter_fixture.hpp"
#include "uqm/orbital_encounter.hpp"
#include "uqm/world_archive.hpp"
#include "uqm/system_flight.hpp"
#include <boost/test/unit_test.hpp>
#include <stdexcept>

namespace {
std::shared_ptr<uqm::GameState> campaign() {
    return std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Player"});
}
}
BOOST_AUTO_TEST_CASE(encounter_borrows_and_archives_use_their_owner_with_another_session_selected)
{
    uqm::GameSession owner{campaign(), uqm::SinglePlayerId}, other{campaign(), uqm::SinglePlayerId};
    NativeEncounterQueues first{owner}, second{other};
    uqm::SessionActivation unrelated{other};
    first.campaign.activity = IN_INTERPLANETARY;
    second.campaign.activity = IN_HYPERSPACE;
    first.addGroup(1).orbit_pos = 7;
    const auto untouched = other.world().snapshot();
    {
        uqm::OrbitalEncounter encounter{owner};
        BOOST_TEST(CountLinks(&first.campaign.groups) == 0u);
        first.addShip(9);
        first.campaign.activity |= START_INTERPLANETARY;
        encounter.restore();
        encounter.restore();
        BOOST_TEST(CountLinks(&first.campaign.ships) == 0u);
        BOOST_TEST(CountLinks(&first.campaign.groups) == 1u);
        const auto &group = *LockIpGroup(&first.campaign.groups, GetHeadLink(&first.campaign.groups));
        BOOST_TEST(group.loc.x == 123);
        BOOST_TEST(group.loc.y == -456);
        BOOST_TEST(group.orbit_pos == 7u);
        BOOST_TEST(group.task == EXPLORE);
        BOOST_TEST(!(first.campaign.activity & START_INTERPLANETARY));
    }
    BOOST_TEST(second.campaign.activity == IN_HYPERSPACE);
    BOOST_TEST(CountLinks(&second.campaign.groups) == 0u);
    BOOST_TEST(other.world().snapshot().randomGroups == untouched.randomGroups, boost::test_tools::per_element());
}

BOOST_AUTO_TEST_CASE(orbital_restoration_unwinds_normally_but_preserves_load_and_abort_handoffs)
{
    for (const auto flag : {ACTIVITY{0}, ACTIVITY{CHECK_LOAD}, ACTIVITY{CHECK_ABORT}}) {
        uqm::GameSession session{campaign(), uqm::SinglePlayerId};
        NativeEncounterQueues queues{session};
        queues.addGroup(2);
        try {
            uqm::OrbitalEncounter encounter{session};
            queues.addShip(11);
            queues.campaign.activity = START_INTERPLANETARY | flag;
            throw std::runtime_error("leave script");
        } catch (const std::runtime_error &) {}
        BOOST_TEST(CountLinks(&queues.campaign.groups) == (flag ? 0u : 1u));
        BOOST_TEST(CountLinks(&queues.campaign.ships) == (flag ? 1u : 0u));
        BOOST_TEST(bool(queues.campaign.activity & START_INTERPLANETARY) == bool(flag));
        BOOST_TEST(!session.world().snapshot().randomGroups.empty());
    }
}

BOOST_AUTO_TEST_CASE(group_archives_reconcile_survivors_and_keep_last_encounter_per_session)
{
    uqm::GameSession owner{campaign(), uqm::SinglePlayerId}, other{campaign(), uqm::SinglePlayerId};
    NativeEncounterQueues first{owner}, second{other};
    uqm::SessionActivation unrelated{other};
    first.addGroup(1);
    first.addShip(17).captains_name_index = NAME_OFFSET + 3;
    owner.groupArchive().save(GROUPS_RANDOM, 1);
    ReinitQueue(&first.campaign.ships);
    owner.groupArchive().save(GROUPS_RANDOM, GROUP_SAVE_IP);
    BOOST_TEST(owner.groupArchive().load(GROUPS_RANDOM, 1));
    BOOST_TEST(CountLinks(&first.campaign.groups) == 0u);
    auto &survivor = *LockShipFrag(&first.campaign.ships, GetHeadLink(&first.campaign.ships));
    BOOST_TEST(survivor.crew_level == 17u);
    BOOST_TEST(survivor.captains_name_index == NAME_OFFSET + 3u);
    survivor.crew_level = 4;
    // Interleaving another owner's save must not erase the first owner's group ID.
    second.addGroup(2);
    other.groupArchive().save(GROUPS_RANDOM, GROUP_SAVE_IP);
    BOOST_TEST(owner.groupArchive().load(GROUPS_RANDOM, GROUP_LOAD_IP));
    BOOST_TEST(CountLinks(&first.campaign.ships) == 0u);
    BOOST_TEST(owner.groupArchive().load(GROUPS_RANDOM, 1));
    BOOST_TEST(LockShipFrag(&first.campaign.ships, GetHeadLink(&first.campaign.ships))->crew_level == 4u);
    // Wiping out the fleet removes its navigation group when the list resumes.
    ReinitQueue(&first.campaign.ships);
    BOOST_TEST(!owner.groupArchive().load(GROUPS_RANDOM, GROUP_LOAD_IP));
    BOOST_TEST(CountLinks(&first.campaign.groups) == 0u);
    BOOST_TEST(!owner.groupArchive().load(GROUPS_RANDOM, 1));
}

BOOST_AUTO_TEST_CASE(scripted_groups_and_archive_expiry_use_the_owning_campaign)
{
    uqm::GameSession owner{campaign(), uqm::SinglePlayerId}, other{campaign(), uqm::SinglePlayerId};
    NativeEncounterQueues first{owner}, second{other};
    uqm::SessionActivation unrelated{other};
    owner.clock().advance(5);
    owner.player().navigationState().currentStar = StarId{7};
    first.addShip(INFINITE_FLEET);
    const auto offset = owner.groupArchive().save(GROUPS_ADD_NEW, 1);
    BOOST_TEST(offset > 0u);
    ReinitQueue(&first.campaign.ships);
    first.campaign.battleGroupRef = offset;
    first.addGroup(1);
    owner.groupArchive().save(GROUPS_RANDOM, GROUP_SAVE_IP);
    BOOST_TEST(owner.groupArchive().load(offset, 1));
    // Preserve the existing one-byte group encoding, including sentinel narrowing.
    BOOST_TEST(LockShipFrag(&first.campaign.ships, GetHeadLink(&first.campaign.ships))->crew_level
        == static_cast<BYTE>(INFINITE_FLEET));
    BOOST_TEST(owner.groupArchive().load(GROUPS_RANDOM, GROUP_LOAD_IP));
    auto *file = uqm::detail::openStateFile(owner.world(), RANDGRPINFO_FILE, "rb");
    GROUP_HEADER header{};
    ReadGroupHeader(file, &header);
    CloseStateFile(file);
    const auto expiry = owner.clock().resolve({0, 0, 7});
    BOOST_TEST(header.star_index == 7u);
    BOOST_TEST(header.day_index == expiry.day);
    BOOST_TEST(header.month_index == expiry.month);
    BOOST_TEST(header.year_index == expiry.year);
    BOOST_CHECK_THROW(owner.groupArchive().load(GROUPS_RANDOM, GROUP_INIT_IP), std::invalid_argument);
    BOOST_CHECK_THROW(owner.groupArchive().save(GROUPS_RANDOM, 65), std::out_of_range);
    // Throwing after opening an archive must close its cursor before restore.
    owner.player().navigationState().currentStar.reset();
    BOOST_CHECK_THROW(owner.groupArchive().save(GROUPS_RANDOM, 1), std::bad_optional_access);
    BOOST_CHECK_NO_THROW(owner.world().restore(owner.world().snapshot()));
}

BOOST_AUTO_TEST_CASE(random_group_initialization_expires_against_its_owner_clock)
{
    uqm::GameSession owner{campaign(), uqm::SinglePlayerId}, other{campaign(), uqm::SinglePlayerId};
    NativeEncounterQueues first{owner}, second{other};
    uqm::SessionActivation unrelated{other};
    first.addShip(12);
    owner.groupArchive().save(GROUPS_RANDOM, 1);
    ReinitQueue(&first.campaign.ships);
    const auto untouched = other.world().snapshot();
    owner.clock().advance(8);
    PLANET_DESC sun{}, planets[MAX_PLANETS]{};
    sun.NumPlanets = 1;
    BOOST_TEST(!owner.groupArchive().initialize(GROUPS_RANDOM, {sun, planets}, first.campaign.groups));
    BOOST_TEST(CountLinks(&first.campaign.groups) == 0u);
    BOOST_TEST(other.world().snapshot().randomGroups == untouched.randomGroups, boost::test_tools::per_element());
    BOOST_CHECK_NO_THROW(owner.world().restore(owner.world().snapshot()));
}
