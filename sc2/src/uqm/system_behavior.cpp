#include "system_behavior.hpp"
#include "gendef.h"
#include "planets/generate/gendefault.h"
namespace uqm {
bool SystemBehavior::initNpcs(SOLARSYS_STATE &system) { return GenerateDefault_initNpcs(&system); }
bool SystemBehavior::reinitNpcs(SOLARSYS_STATE &system) { return GenerateDefault_reinitNpcs(&system); }
bool SystemBehavior::uninitNpcs(SOLARSYS_STATE &system) { return GenerateDefault_uninitNpcs(&system); }
bool SystemBehavior::generatePlanets(SOLARSYS_STATE &system) { return GenerateDefault_generatePlanets(&system); }
bool SystemBehavior::generateMoons(SOLARSYS_STATE &system, PLANET_DESC &world) { return GenerateDefault_generateMoons(&system, &world); }
bool SystemBehavior::generateName(const SOLARSYS_STATE &system, const PLANET_DESC &world) { return GenerateDefault_generateName(&system, &world); }
bool SystemBehavior::generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &world) { return GenerateDefault_generateOrbital(&system, &world); }
COUNT SystemBehavior::generateMinerals(const SOLARSYS_STATE &system, const PLANET_DESC &world, COUNT node, NODE_INFO *info) { return GenerateDefault_generateMinerals(&system, &world, node, info); }
COUNT SystemBehavior::generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &world, COUNT node, NODE_INFO *info) { return GenerateDefault_generateEnergy(&system, &world, node, info); }
COUNT SystemBehavior::generateLife(const SOLARSYS_STATE &system, const PLANET_DESC &world, COUNT node, NODE_INFO *info) { return GenerateDefault_generateLife(&system, &world, node, info); }
bool SystemBehavior::pickupMinerals(SOLARSYS_STATE &system, PLANET_DESC &world, COUNT node) { return GenerateDefault_pickupMinerals(&system, &world, node); }
bool SystemBehavior::pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &world, COUNT node) { return GenerateDefault_pickupEnergy(&system, &world, node); }
bool SystemBehavior::pickupLife(SOLARSYS_STATE &system, PLANET_DESC &world, COUNT node) { return GenerateDefault_pickupLife(&system, &world, node); }
} // namespace uqm
