#include "system_behavior.hpp"
#include "gendef.h"
#include "starmap.h"
#include <stdexcept>
#include "planets/generate/gendefault.h"
namespace uqm {
void SystemBehavior::attachStar(const STAR_DESC &star) {
    if (star_ && star_ != &star) throw std::logic_error("cannot rebind system catalogue context");
    star_ = &star;
}
const STAR_DESC &SystemBehavior::star() const {
    if (!star_) throw std::logic_error("system behavior requires catalogue context");
    return *star_;
}

bool SystemBehavior::initNpcs(SOLARSYS_STATE &system) { return GenerateDefault_initNpcs(&system); }
bool SystemBehavior::reinitNpcs(SOLARSYS_STATE &system) { return GenerateDefault_reinitNpcs(&system); }
bool SystemBehavior::uninitNpcs(SOLARSYS_STATE &system) { return GenerateDefault_uninitNpcs(&system); }
bool SystemBehavior::generatePlanets(SOLARSYS_STATE &system) { return GenerateDefault_generatePlanets(&system); }
bool SystemBehavior::generateMoons(SOLARSYS_STATE &system, PLANET_DESC &world) { return GenerateDefault_generateMoons(&system, &world); }
bool SystemBehavior::generateName(const SOLARSYS_STATE &system, const PLANET_DESC &world) { return GenerateDefault_generateName(&system, &world); }
COUNT SystemBehavior::generateMinerals(const SOLARSYS_STATE &system, const PLANET_DESC &world, COUNT node, NODE_INFO *info) { return GenerateDefault_generateMinerals(&system, &world, node, info); }
COUNT SystemBehavior::generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &world, COUNT node, NODE_INFO *info) { return GenerateDefault_generateEnergy(&system, &world, node, info); }
COUNT SystemBehavior::generateLife(const SOLARSYS_STATE &system, const PLANET_DESC &world, COUNT node, NODE_INFO *info) { return GenerateDefault_generateLife(&system, &world, node, info); }
bool SystemBehavior::pickupMinerals(SOLARSYS_STATE &system, PLANET_DESC &world, COUNT node) { return GenerateDefault_pickupMinerals(&system, &world, node); }
bool SystemBehavior::pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &world, COUNT node) { return GenerateDefault_pickupEnergy(&system, &world, node); }
bool SystemBehavior::pickupLife(SOLARSYS_STATE &system, PLANET_DESC &world, COUNT node) { return GenerateDefault_pickupLife(&system, &world, node); }
} // namespace uqm
