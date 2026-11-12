#ifndef UQM_GAME_COLLECTIONS_HPP_
#define UQM_GAME_COLLECTIONS_HPP_
#include "collection_storage.hpp"
#include "element.h"
#include "fleet.hpp"
#include "grpinfo.h"
#include "master.h"
namespace uqm {
// Distinct typed collection classes, sharing the stable-address storage contract.
// Existing Fleet, RuntimeShipRoster and SimulationEngine own the first three.
template<class Storage = std::vector<ELEMENT>> using SimulationElements = CollectionStorage<ELEMENT, Storage>;
template<class Storage = std::vector<STARSHIP>> using BattleParticipants = CollectionStorage<STARSHIP, Storage>;
template<class Storage = std::vector<Ship>> using FleetShips = CollectionStorage<Ship, Storage>;
template<class Storage = std::vector<FLEET_INFO>> using RaceFleets = CollectionStorage<FLEET_INFO, Storage>;
template<class Storage = std::vector<IP_GROUP>> using SystemGroups = CollectionStorage<IP_GROUP, Storage>;
template<class Storage = std::vector<MASTER_SHIP_INFO>> using MasterShips = CollectionStorage<MASTER_SHIP_INFO, Storage>;
namespace detail {
// Checked C-buffer borrows. The queue must have the matching collection kind;
// returned collection references end at UninitQueue. Native traversal uses the
// active vector, while legacy callers see synchronized link projections.
const FleetShips<> &fleetShips(QUEUE &queue);
const FleetShips<> &fleetShips(const QUEUE &queue);
const BattleParticipants<> &battleParticipants(QUEUE &queue);
const BattleParticipants<> &battleParticipants(const QUEUE &queue);
const RaceFleets<> &raceFleets(QUEUE &queue);
const RaceFleets<> &raceFleets(const QUEUE &queue);
const SystemGroups<> &systemGroups(QUEUE &queue);
}
}
#endif
