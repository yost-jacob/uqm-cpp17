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
namespace {
// Temporary bridge for unmigrated special systems. Native rules never dispatch
// back through this table. Its static functions retain their existing C context.
class LegacySystemBehavior final : public SystemBehavior {
public:
    LegacySystemBehavior(CampaignState &campaign, const GenerateFunctions &functions)
        : SystemBehavior(campaign), functions_(functions) {}
    bool initNpcs(SOLARSYS_STATE &system) override { return functions_.initNpcs(&system); }
    bool reinitNpcs(SOLARSYS_STATE &system) override { return functions_.reinitNpcs(&system); }
    bool uninitNpcs(SOLARSYS_STATE &system) override { return functions_.uninitNpcs(&system); }
    bool generatePlanets(SOLARSYS_STATE &system) override { return functions_.generatePlanets(&system); }
    bool generateMoons(SOLARSYS_STATE &system, PLANET_DESC &world) override { return functions_.generateMoons(&system, &world); }
    bool generateName(const SOLARSYS_STATE &system, const PLANET_DESC &world) override { return functions_.generateName(&system, &world); }
    bool generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &world) override { return functions_.generateOrbital(&system, &world); }
    COUNT generateMinerals(const SOLARSYS_STATE &system, const PLANET_DESC &world, COUNT node, NODE_INFO *info) override { return functions_.generateMinerals(&system, &world, node, info); }
    COUNT generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &world, COUNT node, NODE_INFO *info) override { return functions_.generateEnergy(&system, &world, node, info); }
    COUNT generateLife(const SOLARSYS_STATE &system, const PLANET_DESC &world, COUNT node, NODE_INFO *info) override { return functions_.generateLife(&system, &world, node, info); }
    bool pickupMinerals(SOLARSYS_STATE &system, PLANET_DESC &world, COUNT node) override { return functions_.pickupMinerals(&system, &world, node); }
    bool pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &world, COUNT node) override { return functions_.pickupEnergy(&system, &world, node); }
    bool pickupLife(SOLARSYS_STATE &system, PLANET_DESC &world, COUNT node) override { return functions_.pickupLife(&system, &world, node); }
private:
    const GenerateFunctions &functions_;
};
}
std::unique_ptr<SystemBehavior> createSystemBehavior(BYTE special, CampaignState &campaign) {
    if (special == CHMMR_DEFINED) return createChmmrSystemBehavior(campaign);
    const auto *legacy = getGenerateFunctions(special);
    if (legacy == &generateDefaultFunctions) return std::make_unique<SystemBehavior>(campaign);
    return std::make_unique<LegacySystemBehavior>(campaign, *legacy);
}
} // namespace uqm
