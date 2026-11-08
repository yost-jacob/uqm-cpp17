#include "uqm/system_behavior.hpp"
#include "uqm/system_runtime.hpp"
#include "uqm/gendef.h"
#include "uqm/globdata.h"
#include "uqm/build.h"
#include "uqm/comm.h"
#include "uqm/state.h"
#include "uqm/sounds.h"
#include "uqm/planets/lander.h"
#include "uqm/planets/generate/gendefault.h"
#include <boost/test/unit_test.hpp>
#include <array>
#include <typeinfo>
namespace {
std::array<unsigned, 13> defaultCalls{};
unsigned legacyCalls = 0, communications = 0, groupsSaved = 0, groupsLoaded = 0, clones = 0;
CONVERSATION lastConversation{};
bool sphereTracked = false;
struct EncounterQueues {
    QUEUE groups{}, ships{};
    ACTIVITY activity = GLOBAL(CurrentActivity);
    EncounterQueues() {
        SwapQueues(&groups, &GLOBAL(ip_group_q));
        SwapQueues(&ships, &GLOBAL(npc_built_ship_q));
        InitQueue(&GLOBAL(ip_group_q), 4, QUEUE_IP_GROUP);
        InitQueue(&GLOBAL(npc_built_ship_q), 4, QUEUE_SHIP_FRAGMENT);
    }
    ~EncounterQueues() {
        UninitQueue(&GLOBAL(ip_group_q));
        UninitQueue(&GLOBAL(npc_built_ship_q));
        SwapQueues(&groups, &GLOBAL(ip_group_q));
        SwapQueues(&ships, &GLOBAL(npc_built_ship_q));
        GLOBAL(CurrentActivity) = activity;
    }
};
void resetCalls() {
    defaultCalls.fill(0);
    legacyCalls = communications = groupsSaved = groupsLoaded = clones = 0;
    sphereTracked = false;
}
bool legacyPlanets(SOLARSYS_STATE *) { ++legacyCalls; return false; }
}
// Dependencies are fake engine services. Tests below use the real behavior
// factory, Chmmr hooks, campaign storage, world ownership and C callback bridge.
extern "C" {
RandomContext SysGenRNG{RandomDefaultSeed};
SOUND MenuSounds = nullptr;
bool GenerateDefault_initNpcs(SOLARSYS_STATE *system) {
    ++defaultCalls[0];
    (void)system;
    return true;
}
bool GenerateDefault_reinitNpcs(SOLARSYS_STATE *system) {
    ++defaultCalls[1];
    (void)system;
    return true;
}
bool GenerateDefault_uninitNpcs(SOLARSYS_STATE *system) {
    ++defaultCalls[2];
    (void)system;
    return true;
}
bool GenerateDefault_generatePlanets(SOLARSYS_STATE *system) {
    ++defaultCalls[3];
    (void)system;
    system->SunDesc[0].NumPlanets = 2;
    return true;
}
bool GenerateDefault_generateMoons(SOLARSYS_STATE *system, PLANET_DESC *world) {
    ++defaultCalls[4];
    (void)system;
    (void)world;
    return true;
}
bool GenerateDefault_generateName(const SOLARSYS_STATE *system, const PLANET_DESC *world) {
    ++defaultCalls[5];
    (void)system;
    (void)world;
    return true;
}
bool GenerateDefault_generateOrbital(SOLARSYS_STATE *system, PLANET_DESC *world) {
    ++defaultCalls[6];
    (void)system;
    (void)world;
    return true;
}
COUNT GenerateDefault_generateMinerals(const SOLARSYS_STATE *system, const PLANET_DESC *world, COUNT node, NODE_INFO *info) {
    ++defaultCalls[7];
    (void)system;
    (void)world;
    (void)node;
    (void)info;
    return node;
}
COUNT GenerateDefault_generateEnergy(const SOLARSYS_STATE *system, const PLANET_DESC *world, COUNT node, NODE_INFO *info) {
    ++defaultCalls[8];
    (void)system;
    (void)world;
    (void)node;
    (void)info;
    return node;
}
COUNT GenerateDefault_generateLife(const SOLARSYS_STATE *system, const PLANET_DESC *world, COUNT node, NODE_INFO *info) {
    ++defaultCalls[9];
    (void)system;
    (void)world;
    (void)node;
    (void)info;
    return node;
}
bool GenerateDefault_pickupMinerals(SOLARSYS_STATE *system, PLANET_DESC *world, COUNT node) {
    ++defaultCalls[10];
    (void)system;
    (void)world;
    (void)node;
    return true;
}
bool GenerateDefault_pickupEnergy(SOLARSYS_STATE *system, PLANET_DESC *world, COUNT node) {
    ++defaultCalls[11];
    (void)system;
    (void)world;
    (void)node;
    return true;
}
bool GenerateDefault_pickupLife(SOLARSYS_STATE *system, PLANET_DESC *world, COUNT node) {
    ++defaultCalls[12];
    (void)system;
    (void)world;
    (void)node;
    return true;
}
const GenerateFunctions generateDefaultFunctions = {
    GenerateDefault_initNpcs,
    GenerateDefault_reinitNpcs,
    GenerateDefault_uninitNpcs,
    GenerateDefault_generatePlanets,
    GenerateDefault_generateMoons,
    GenerateDefault_generateName,
    GenerateDefault_generateOrbital,
    GenerateDefault_generateMinerals,
    GenerateDefault_generateEnergy,
    GenerateDefault_generateLife,
    GenerateDefault_pickupMinerals,
    GenerateDefault_pickupEnergy,
    GenerateDefault_pickupLife,
};
const GenerateFunctions *getGenerateFunctions(BYTE special) {
    static const auto legacy = [] {
        auto functions = generateDefaultFunctions;
        functions.generatePlanets = legacyPlanets;
        return functions;
    }();
    return special == SOL_DEFINED ? &legacy : &generateDefaultFunctions;
}
bool matchWorld(const SOLARSYS_STATE *system, const PLANET_DESC *world, BYTE planet, BYTE moon) {
    if (moon == MATCH_PLANET) return world == &system->PlanetDesc[planet];
    return world == &system->MoonDesc[moon] && world->pPrevDesc == &system->PlanetDesc[planet];
}
COUNT InitCommunication(CONVERSATION conversation) { ++communications; lastConversation = conversation; return 0; }
COUNT StartSphereTracking(COUNT) { return sphereTracked ? 1 : 0; }
DWORD PutGroupInfo(DWORD, BYTE) { ++groupsSaved; return 0; }
BOOLEAN GetGroupInfo(DWORD, BYTE) { ++groupsLoaded; return TRUE; }
HSHIPFRAG CloneShipFragment(COUNT, QUEUE *, COUNT) { ++clones; return nullptr; }
void LoadStdLanderFont(PLANET_INFO *) {}
void FreeLanderFont(PLANET_INFO *) {}
void DoDiscoveryReport(SOUND) {}
}

BOOST_AUTO_TEST_CASE(system_factory_selects_native_chmmr_default_and_legacy_rules)
{
    uqm::GameState state{{uqm::SinglePlayerId, "Player"}};
    auto &campaign = state.player(uqm::SinglePlayerId).campaign();
    auto ordinary = uqm::createSystemBehavior(0, campaign);
    auto special = uqm::createSystemBehavior(CHMMR_DEFINED, campaign);
    auto legacy = uqm::createSystemBehavior(SOL_DEFINED, campaign);
    BOOST_TEST((typeid(*ordinary) == typeid(uqm::SystemBehavior)));
    BOOST_TEST((typeid(*special) != typeid(*ordinary)));
    SOLARSYS_STATE system{};
    resetCalls();
    BOOST_TEST(!legacy->generatePlanets(system));
    BOOST_TEST(legacyCalls == 1u);
    BOOST_TEST(defaultCalls[3] == 0u);
    BOOST_TEST(special->generatePlanets(system));
    BOOST_TEST(defaultCalls[3] == 1u);
    BOOST_TEST(system.PlanetDesc[1].NumPlanets == 1u);
}
BOOST_AUTO_TEST_CASE(chmmr_hooks_read_the_borrowed_campaign_at_action_time)
{
    uqm::GameState alice{{uqm::SinglePlayerId, "Alice"}}, bob{{uqm::SinglePlayerId, "Bob"}};
    auto &a = alice.player(uqm::SinglePlayerId).campaign();
    auto &b = bob.player(uqm::SinglePlayerId).campaign();
    auto first = uqm::createSystemBehavior(CHMMR_DEFINED, a);
    auto second = uqm::createSystemBehavior(CHMMR_DEFINED, b);
    SOLARSYS_STATE system{};
    resetCalls();
    b.set(uqm::StoryFlag::ChmmrFreed);
    first->generatePlanets(system);
    BOOST_TEST((system.PlanetDesc[1].data_index & PLANET_SHIELDED) != 0);
    second->generatePlanets(system);
    BOOST_TEST((system.PlanetDesc[1].data_index & PLANET_SHIELDED) == 0);
    a.set(uqm::StoryFlag::ChmmrFreed); // Construction did not cache the flag.
    a.set(uqm::StoryCounter::BombProgress, 2);
    const auto oldActivity = GLOBAL(CurrentActivity);
    GLOBAL(CurrentActivity) = 0;
    first->generateOrbital(system, system.PlanetDesc[1]);
    BOOST_TEST(communications == 1u);
    BOOST_TEST(static_cast<int>(lastConversation) == static_cast<int>(CHMMR_CONVERSATION));
    BOOST_TEST(a.read(uqm::StoryCounter::EncounterContext) == 128u);
    BOOST_TEST(b.read(uqm::StoryCounter::EncounterContext) == 0u);
    BOOST_TEST((GLOBAL(CurrentActivity) & END_INTERPLANETARY) != 0);
    BOOST_TEST(defaultCalls[6] == 0u); // Handled special orbit never falls through.
    GLOBAL(CurrentActivity) = oldActivity;
}
BOOST_AUTO_TEST_CASE(chmmr_ilwrath_gate_and_ordinary_orbits_preserve_fallback_order)
{
    uqm::GameState state{{uqm::SinglePlayerId, "Player"}};
    auto &campaign = state.player(uqm::SinglePlayerId).campaign();
    auto behavior = uqm::createSystemBehavior(CHMMR_DEFINED, campaign);
    SOLARSYS_STATE system{};
    resetCalls();
    behavior->generateOrbital(system, system.PlanetDesc[0]);
    BOOST_TEST(defaultCalls[6] == 1u);
    campaign.set(uqm::StoryFlag::SunDeviceOnShip);
    campaign.set(uqm::StoryFlag::IlwrathDeceived);
    EncounterQueues queues;
    sphereTracked = true;
    behavior->generateOrbital(system, system.PlanetDesc[1]);
    BOOST_TEST(communications == 0u);
    campaign.clear(uqm::StoryFlag::IlwrathDeceived);
    const auto oldActivity = GLOBAL(CurrentActivity);
    GLOBAL(CurrentActivity) = 0;
    behavior->generateOrbital(system, system.PlanetDesc[1]);
    BOOST_TEST(communications == 1u);
    BOOST_TEST(static_cast<int>(lastConversation) == static_cast<int>(ILWRATH_CONVERSATION));
    BOOST_TEST(campaign.read(uqm::StoryCounter::EncounterContext) == 64u);
    BOOST_TEST(groupsSaved == 1u);
    BOOST_TEST(groupsLoaded == 1u);
    BOOST_TEST(clones == 1u);
    BOOST_TEST((GLOBAL(CurrentActivity) & START_INTERPLANETARY) == 0);
    GLOBAL(CurrentActivity) = oldActivity;
}
BOOST_AUTO_TEST_CASE(system_keeps_behavior_identity_when_generated_layout_is_attached)
{
    uqm::GameState state{{uqm::SinglePlayerId, "Player"}};
    auto &campaign = state.player(uqm::SinglePlayerId).campaign();
    UQM_SYSTEM_RUNTIME context{campaign, state.worldOwner(), nullptr};
    SOLARSYS_STATE system{};
    system.worlds = &context;
    // Exercise native ownership/layout independently of catalogue loading.
    context.system = std::make_unique<uqm::SolarSystem>(StarId{42}, context.world,
            campaign, CHMMR_DEFINED);
    auto *identity = &context.system->behavior();
    resetCalls();
    identity->generatePlanets(system);
    const uqm::WorldId id{context.world->id(), StarId{42}, 0, std::nullopt};
    context.system->replacePlanets({{id, uqm::WorldKind::Planet, 123, false, "First", 1}});
    BOOST_TEST(&context.system->behavior() == identity);
    BOOST_TEST(context.system->worldCount() == 1u);
    auto invalid = id;
    invalid.planet = 2;
    BOOST_CHECK_THROW(context.system->replacePlanets({{invalid, uqm::WorldKind::Planet, 123, false, "Bad"}}), std::invalid_argument);
    BOOST_TEST(context.system->worldCount() == 1u);
    BOOST_TEST(context.system->world(0).definition().name == "First");
    campaign.set(uqm::StoryFlag::ChmmrFreed);
    identity->generatePlanets(system);
    BOOST_TEST((system.PlanetDesc[1].data_index & PLANET_SHIELDED) == 0);
}
BOOST_AUTO_TEST_CASE(default_system_hooks_forward_all_thirteen_existing_contracts)
{
    uqm::GameState state{{uqm::SinglePlayerId, "Player"}};
    uqm::SystemBehavior behavior{state.player(uqm::SinglePlayerId).campaign()};
    SOLARSYS_STATE system{};
    PLANET_DESC world{};
    resetCalls();
    BOOST_TEST(behavior.initNpcs(system) == true);
    BOOST_TEST(behavior.reinitNpcs(system) == true);
    BOOST_TEST(behavior.uninitNpcs(system) == true);
    BOOST_TEST(behavior.generatePlanets(system) == true);
    BOOST_TEST(behavior.generateMoons(system, world) == true);
    BOOST_TEST(behavior.generateName(system, world) == true);
    BOOST_TEST(behavior.generateOrbital(system, world) == true);
    BOOST_TEST(behavior.generateMinerals(system, world, COUNT{7}, nullptr) == 7u);
    BOOST_TEST(behavior.generateEnergy(system, world, COUNT{7}, nullptr) == 7u);
    BOOST_TEST(behavior.generateLife(system, world, COUNT{7}, nullptr) == 7u);
    BOOST_TEST(behavior.pickupMinerals(system, world, COUNT{7}) == true);
    BOOST_TEST(behavior.pickupEnergy(system, world, COUNT{7}) == true);
    BOOST_TEST(behavior.pickupLife(system, world, COUNT{7}) == true);
    for (auto calls : defaultCalls) BOOST_TEST(calls == 1u);
}

// Called under the real catalogue fixture; tests explicit local campaign context
// through the same opaque handle and callback table used by the C game loop.
extern "C" int system_behavior_bridge_contract(STAR_DESC *star) {
    uqm::GameState state{{uqm::SinglePlayerId, "Local campaign"}};
    auto &campaign = state.player(uqm::SinglePlayerId).campaign();
    UQM_SYSTEM_RUNTIME context{campaign, state.worldOwner(), nullptr};
    SOLARSYS_STATE system{};
    system.worlds = &context;
    PrepareSystemWorlds(&system, star);
    auto *identity = &context.system->behavior();
    if (!system.genFuncs->generatePlanets(&system)) return 0;
    if (!(system.PlanetDesc[1].data_index & PLANET_SHIELDED)) return 0;
    BuildSystemWorlds(&system, star);
    if (&context.system->behavior() != identity || context.system->worldCount() != 2) return 0;
    campaign.set(uqm::StoryFlag::ChmmrFreed);
    if (!system.genFuncs->generatePlanets(&system)) return 0;
    if (system.PlanetDesc[1].data_index & PLANET_SHIELDED) return 0;
    if (system.genFuncs->generateMinerals(&system, &system.PlanetDesc[0], 7, nullptr) != 7) return 0;
    return 1;
}
