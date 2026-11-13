#ifndef UQM_TEST_ENCOUNTER_FIXTURE_HPP_
#define UQM_TEST_ENCOUNTER_FIXTURE_HPP_
#include "uqm/game_state.hpp"
#include "uqm/encounter_legacy.hpp"
#include "uqm/build.h"

// Real collections and archive bytes; only presentation/captain services are
// substituted in the test executable. Set up the C race buffer once, then test
// owner-explicit operations while an unrelated session is selected.
struct NativeEncounterQueues {
    uqm::GameSession &session;
    uqm::detail::EncounterLegacyView campaign;
    QUEUE *races = nullptr;
    explicit NativeEncounterQueues(uqm::GameSession &owner)
        : session(owner), campaign(uqm::detail::encounterLegacy(owner.player())) {
        uqm::SessionActivation selected{session};
        races = &GLOBAL(avail_race_q);
        InitQueue(races, NUM_AVAILABLE_RACES, QUEUE_FLEET_INFO);
        InitQueue(&campaign.groups, 16, QUEUE_IP_GROUP);
        InitQueue(&campaign.ships, 16, QUEUE_SHIP_FRAGMENT);
        for (unsigned i = 0; i < NUM_AVAILABLE_RACES; ++i) {
            const auto handle = AllocLink(races);
            auto &race = *LockFleetInfo(races, handle);
            race = {};
            race.SpeciesID = SPATHI_ID;
            race.crew_level = race.max_crew = 30;
            race.max_energy = 10;
            PutQueue(races, handle);
        }
        session.player().navigationState().currentStar = StarId{0};
        session.groupArchive().reset(true);
    }
    ~NativeEncounterQueues() {
        UninitQueue(&campaign.groups);
        UninitQueue(&campaign.ships);
        UninitQueue(races);
    }
    IP_GROUP &addGroup(BYTE id) {
        const auto handle = AllocLink(&campaign.groups);
        auto &group = *LockIpGroup(&campaign.groups, handle);
        group = {};
        group.race_id = SPATHI_SHIP;
        group.group_id = id;
        group.in_system = 1;
        group.loc = {123, -456};
        group.task = EXPLORE;
        PutQueue(&campaign.groups, handle);
        return group;
    }
    SHIP_FRAGMENT &addShip(COUNT crew) {
        const auto handle = CloneShipFragmentFromTemplate(SPATHI_SHIP,
            &session.raceFleets().at(SPATHI_SHIP), &campaign.ships, crew);
        return *LockShipFrag(&campaign.ships, handle);
    }
};
#endif
