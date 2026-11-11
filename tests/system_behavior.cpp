#include "uqm/encounter_runtime.hpp"
#include "uqm/system_behavior.hpp"
#include "uqm/orbital_encounter.hpp"
#include "uqm/system_builders.hpp"
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
#include <functional>
#include <typeinfo>
namespace {
std::array<unsigned, 13> defaultCalls{};
unsigned communications = 0, groupsSaved = 0, groupsLoaded = 0, clones = 0;
CONVERSATION lastConversation{};
bool sphereTracked = false;
std::function<void()> dialogueHook;
uqm::StoryState *encounterCampaign = nullptr;
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
    communications = groupsSaved = groupsLoaded = clones = 0;
    sphereTracked = false;
}
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
bool matchWorld(const SOLARSYS_STATE *system, const PLANET_DESC *world, BYTE planet, BYTE moon) {
    if (moon == MATCH_PLANET) return world == &system->PlanetDesc[planet];
    return world == &system->MoonDesc[moon] && world->pPrevDesc == &system->PlanetDesc[planet];
}
COUNT InitCommunication(CONVERSATION conversation) {
    ++communications; lastConversation = conversation;
    if (dialogueHook) dialogueHook();
    return 0;
}
COUNT StartSphereTracking(COUNT) { return sphereTracked ? 1 : 0; }
DWORD PutGroupInfo(DWORD, BYTE) { ++groupsSaved; return 0; }
BOOLEAN GetGroupInfo(DWORD, BYTE) { ++groupsLoaded; return TRUE; }
HSHIPFRAG CloneShipFragment(COUNT, QUEUE *, COUNT) { ++clones; return nullptr; }
void LoadStdLanderFont(PLANET_INFO *) {}
void FreeLanderFont(PLANET_INFO *) {}
void DoDiscoveryReport(SOUND) {}
}

namespace { std::string selectedBuilder; }
namespace uqm {
std::unique_ptr<SystemBehavior> createAndrosynthSystemBehavior(StoryState &story) {
    selectedBuilder = "Androsynth";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createBurvixeseSystemBehavior(StoryState &story) {
    selectedBuilder = "Burvixese";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createColonySystemBehavior(StoryState &story) {
    selectedBuilder = "Colony";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createDruugeSystemBehavior(StoryState &story) {
    selectedBuilder = "Druuge";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createIlwrathSystemBehavior(StoryState &story) {
    selectedBuilder = "Ilwrath";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createMelnormeSystemBehavior(StoryState &story) {
    selectedBuilder = "Melnorme";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createMyconSystemBehavior(StoryState &story) {
    selectedBuilder = "Mycon";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createOrzSystemBehavior(StoryState &story) {
    selectedBuilder = "Orz";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createTalkingPetSystemBehavior(StoryState &story) {
    selectedBuilder = "TalkingPet";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createPkunkSystemBehavior(StoryState &story) {
    selectedBuilder = "Pkunk";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createRainbowWorldSystemBehavior(StoryState &story) {
    selectedBuilder = "RainbowWorld";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createSaMatraSystemBehavior(StoryState &story) {
    selectedBuilder = "SaMatra";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createShofixtiSystemBehavior(StoryState &story) {
    selectedBuilder = "Shofixti";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createSlylandroSystemBehavior(StoryState &story) {
    selectedBuilder = "Slylandro";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createSolSystemBehavior(StoryState &story) {
    selectedBuilder = "Sol";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createSpathiSystemBehavior(StoryState &story) {
    selectedBuilder = "Spathi";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createSupoxSystemBehavior(StoryState &story) {
    selectedBuilder = "Supox";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createThraddashSystemBehavior(StoryState &story) {
    selectedBuilder = "Thraddash";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createTrapSystemBehavior(StoryState &story) {
    selectedBuilder = "Trap";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createUtwigSystemBehavior(StoryState &story) {
    selectedBuilder = "Utwig";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createVaultSystemBehavior(StoryState &story) {
    selectedBuilder = "Vault";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createVuxSystemBehavior(StoryState &story) {
    selectedBuilder = "Vux";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createWreckSystemBehavior(StoryState &story) {
    selectedBuilder = "Wreck";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createYehatSystemBehavior(StoryState &story) {
    selectedBuilder = "Yehat";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createZoqFotPikScoutSystemBehavior(StoryState &story) {
    selectedBuilder = "ZoqFotPikScout";
    return std::make_unique<SystemBehavior>(story);
}
std::unique_ptr<SystemBehavior> createZoqFotPikSystemBehavior(StoryState &story) {
    selectedBuilder = "ZoqFotPik";
    return std::make_unique<SystemBehavior>(story);
}
}
BOOST_AUTO_TEST_CASE(system_factory_selects_native_chmmr_and_default_rules)
{
    uqm::GameState state{{uqm::SinglePlayerId, "Player"}};
    auto &story = state.player(uqm::SinglePlayerId).story();
    auto ordinary = uqm::createSystemBehavior(0, story);
    auto special = uqm::createSystemBehavior(CHMMR_DEFINED, story);
    BOOST_TEST((typeid(*ordinary) == typeid(uqm::SystemBehavior)));
    BOOST_TEST((typeid(*special) != typeid(*ordinary)));
    SOLARSYS_STATE system{};
    resetCalls();
    BOOST_TEST(special->generatePlanets(system));
    BOOST_TEST(defaultCalls[3] == 1u);
    BOOST_TEST(system.PlanetDesc[1].NumPlanets == 1u);
}
BOOST_AUTO_TEST_CASE(chmmr_hooks_read_the_borrowed_campaign_at_action_time)
{
    uqm::GameState alice{{uqm::SinglePlayerId, "Alice"}}, bob{{uqm::SinglePlayerId, "Bob"}};
    auto &a = alice.player(uqm::SinglePlayerId).story();
    auto &b = bob.player(uqm::SinglePlayerId).story();
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
    auto &story = state.player(uqm::SinglePlayerId).story();
    auto behavior = uqm::createSystemBehavior(CHMMR_DEFINED, story);
    SOLARSYS_STATE system{};
    resetCalls();
    behavior->generateOrbital(system, system.PlanetDesc[0]);
    BOOST_TEST(defaultCalls[6] == 1u);
    story.set(uqm::StoryFlag::SunDeviceOnShip);
    story.set(uqm::StoryFlag::IlwrathDeceived);
    EncounterQueues queues;
    sphereTracked = true;
    behavior->generateOrbital(system, system.PlanetDesc[1]);
    BOOST_TEST(communications == 0u);
    story.clear(uqm::StoryFlag::IlwrathDeceived);
    const auto oldActivity = GLOBAL(CurrentActivity);
    GLOBAL(CurrentActivity) = 0;
    behavior->generateOrbital(system, system.PlanetDesc[1]);
    BOOST_TEST(communications == 1u);
    BOOST_TEST(static_cast<int>(lastConversation) == static_cast<int>(ILWRATH_CONVERSATION));
    BOOST_TEST(story.read(uqm::StoryCounter::EncounterContext) == 64u);
    BOOST_TEST(groupsSaved == 1u);
    BOOST_TEST(groupsLoaded == 1u);
    BOOST_TEST(clones == 1u);
    BOOST_TEST((GLOBAL(CurrentActivity) & START_INTERPLANETARY) == 0);
    GLOBAL(CurrentActivity) = oldActivity;
}
BOOST_AUTO_TEST_CASE(system_keeps_behavior_identity_when_generated_layout_is_attached)
{
    uqm::GameState state{{uqm::SinglePlayerId, "Player"}};
    auto &story = state.player(uqm::SinglePlayerId).story();
    UQM_SYSTEM_RUNTIME context{story, state.worldOwner(), nullptr};
    SOLARSYS_STATE system{};
    system.worlds = &context;
    // Exercise native ownership/layout independently of catalogue loading.
    context.system = std::make_unique<uqm::SolarSystem>(StarId{42}, context.world,
            story, CHMMR_DEFINED);
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
    story.set(uqm::StoryFlag::ChmmrFreed);
    identity->generatePlanets(system);
    BOOST_TEST((system.PlanetDesc[1].data_index & PLANET_SHIELDED) == 0);
}
BOOST_AUTO_TEST_CASE(default_system_hooks_forward_all_thirteen_existing_contracts)
{
    uqm::GameState state{{uqm::SinglePlayerId, "Player"}};
    uqm::SystemBehavior behavior{state.player(uqm::SinglePlayerId).story()};
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
    auto &story = state.player(uqm::SinglePlayerId).story();
    UQM_SYSTEM_RUNTIME context{story, state.worldOwner(), nullptr};
    SOLARSYS_STATE system{};
    system.worlds = &context;
    PrepareSystemWorlds(&system, star);
    auto *identity = &context.system->behavior();
    if (!uqm::systemBehavior(system).generatePlanets(system)) return 0;
    if (!(system.PlanetDesc[1].data_index & PLANET_SHIELDED)) return 0;
    BuildSystemWorlds(&system, star);
    if (&context.system->behavior() != identity || context.system->worldCount() != 2) return 0;
    story.set(uqm::StoryFlag::ChmmrFreed);
    if (!uqm::systemBehavior(system).generatePlanets(system)) return 0;
    if (system.PlanetDesc[1].data_index & PLANET_SHIELDED) return 0;
    if (uqm::systemBehavior(system).generateMinerals(system, system.PlanetDesc[0], 7, nullptr) != 7) return 0;
    return 1;
}

BOOST_AUTO_TEST_CASE(system_factory_preserves_catalogue_special_system_mappings)
{
    uqm::GameState state{{uqm::SinglePlayerId, "Player"}};
    auto &story = state.player(uqm::SinglePlayerId).story();
    struct Mapping { BYTE id; const char *builder; };
    const Mapping expected[] = {
        {SOL_DEFINED, "Sol"},
        {SHOFIXTI_DEFINED, "Shofixti"},
        {START_COLONY_DEFINED, "Colony"},
        {SPATHI_DEFINED, "Spathi"},
        {MELNORME0_DEFINED, "Melnorme"},
        {MELNORME1_DEFINED, "Melnorme"},
        {MELNORME2_DEFINED, "Melnorme"},
        {MELNORME3_DEFINED, "Melnorme"},
        {MELNORME4_DEFINED, "Melnorme"},
        {MELNORME5_DEFINED, "Melnorme"},
        {MELNORME6_DEFINED, "Melnorme"},
        {MELNORME7_DEFINED, "Melnorme"},
        {MELNORME8_DEFINED, "Melnorme"},
        {TALKING_PET_DEFINED, "TalkingPet"},
        {MYCON_TRAP_DEFINED, "Trap"},
        {BURVIXESE_DEFINED, "Burvixese"},
        {SLYLANDRO_DEFINED, "Slylandro"},
        {DRUUGE_DEFINED, "Druuge"},
        {BOMB_DEFINED, "Utwig"},
        {UTWIG_DEFINED, "Utwig"},
        {AQUA_HELIX_DEFINED, "Thraddash"},
        {THRADD_DEFINED, "Thraddash"},
        {SUN_DEVICE_DEFINED, "Mycon"},
        {MYCON_DEFINED, "Mycon"},
        {EGG_CASE0_DEFINED, "Mycon"},
        {EGG_CASE1_DEFINED, "Mycon"},
        {EGG_CASE2_DEFINED, "Mycon"},
        {ANDROSYNTH_DEFINED, "Androsynth"},
        {TAALO_PROTECTOR_DEFINED, "Orz"},
        {ORZ_DEFINED, "Orz"},
        {SHIP_VAULT_DEFINED, "Vault"},
        {URQUAN_WRECK_DEFINED, "Wreck"},
        {MAIDENS_DEFINED, "Vux"},
        {VUX_BEAST_DEFINED, "Vux"},
        {VUX_DEFINED, "Vux"},
        {SAMATRA_DEFINED, "SaMatra"},
        {ZOQFOT_DEFINED, "ZoqFotPik"},
        {ZOQ_SCOUT_DEFINED, "ZoqFotPikScout"},
        {YEHAT_DEFINED, "Yehat"},
        {PKUNK_DEFINED, "Pkunk"},
        {SUPOX_DEFINED, "Supox"},
        {RAINBOW_DEFINED, "RainbowWorld"},
        {ILWRATH_DEFINED, "Ilwrath"},
    };
    for (const auto &mapping : expected) {
        selectedBuilder.clear();
        auto behavior = uqm::createSystemBehavior(mapping.id, story);
        BOOST_REQUIRE(behavior);
        BOOST_TEST(selectedBuilder == mapping.builder);
    }
}
BOOST_AUTO_TEST_CASE(syreen_native_overrides_keep_shielded_planet_and_starbase_moon)
{
    uqm::GameState state{{uqm::SinglePlayerId, "Player"}};
    auto behavior = uqm::createSystemBehavior(SYREEN_DEFINED, state.player(uqm::SinglePlayerId).story());
    SOLARSYS_STATE system{};
    resetCalls();
    behavior->generatePlanets(system);
    BOOST_TEST((system.PlanetDesc[0].data_index & PLANET_SHIELDED) != 0);
    BOOST_TEST(system.PlanetDesc[0].NumPlanets == 1u);
    behavior->generateMoons(system, system.PlanetDesc[0]);
    BOOST_TEST(system.MoonDesc[0].data_index == HIERARCHY_STARBASE);
    BOOST_TEST(defaultCalls[3] == 1u);
    BOOST_TEST(defaultCalls[4] == 1u);
}

namespace uqm { void runExploration(UQM_SYSTEM_RUNTIME &) {} }

namespace {
class TestSystemRule final : public uqm::SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    COUNT encounter() { return communicate(CHMMR_CONVERSATION); }
    BYTE starKind() const { return star().Index; }
};
}
BOOST_AUTO_TEST_CASE(system_encounters_run_as_children_and_propagate_load_requests_after_restoring_parents)
{
    for (const bool load : {false, true}) {
        auto state = std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Player"});
        uqm::GameSession session{state, uqm::SinglePlayerId};
        TestSystemRule rule{session.player().story()};
        rule.attachSession(session);
        struct Parent final : uqm::GameMode {
            uqm::GameSession &session;
            TestSystemRule &rule;
            bool &resumed;
            unsigned &destroyed;
            Parent(uqm::GameSession &s, TestSystemRule &r, bool &after, unsigned &deaths)
                : GameMode(s.dispatcher(), s.nextModeSeed()), session(s), rule(r), resumed(after), destroyed(deaths) {}
            ~Parent() override { ++destroyed; }
            uqm::ModeResult run() override {
                BOOST_TEST(rule.encounter() == 0u);
                resumed = true;
                if (auto exit = session.menuExit()) return *exit;
                return uqm::SessionExit::title();
            }
        };
        const auto oldActivity = GLOBAL(CurrentActivity);
        GLOBAL(CurrentActivity) = 0;
        bool resumed = false;
        unsigned destroyed = 0;
        dialogueHook = [&] {
            BOOST_TEST(encounterCampaign == &session.player().story());
            BOOST_TEST(encounterCampaign != &uqm::gameSession().player().story());
            BOOST_TEST(!resumed);
            BOOST_TEST(destroyed == 0u);
            if (load) session.requestLoad(3);
        };
        const auto exit = session.run(std::make_unique<Parent>(session, rule, resumed, destroyed));
        dialogueHook = {};
        BOOST_TEST(resumed);
        BOOST_TEST(destroyed == 1u);
        BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(load ? uqm::SessionExitKind::LoadGame : uqm::SessionExitKind::ReturnToTitle));
        if (load) {
            BOOST_REQUIRE(exit.loadSlot);
            BOOST_TEST(*exit.loadSlot == 3u);
            BOOST_TEST((GLOBAL(CurrentActivity) & CHECK_LOAD) != 0);
        }
        GLOBAL(CurrentActivity) = oldActivity;
    }
}
BOOST_AUTO_TEST_CASE(system_star_context_is_explicit_and_cannot_be_rebound_mid_visit)
{
    uqm::GameState state{{uqm::SinglePlayerId, "Player"}};
    STAR_DESC first{}, other{};
    first.Index = SOL_DEFINED;
    other.Index = CHMMR_DEFINED;
    TestSystemRule rule{state.player(uqm::SinglePlayerId).story()};
    BOOST_CHECK_THROW(rule.starKind(), std::logic_error);
    rule.attachStar(first);
    BOOST_TEST(rule.starKind() == SOL_DEFINED);
    BOOST_CHECK_THROW(rule.attachStar(other), std::logic_error);
    BOOST_TEST(rule.starKind() == SOL_DEFINED);
}
BOOST_AUTO_TEST_CASE(orbital_encounter_restores_once_at_the_explicit_story_boundary)
{
    EncounterQueues queues;
    resetCalls();
    GLOBAL(CurrentActivity) = 0;
    {
        uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};
        GLOBAL(CurrentActivity) |= START_INTERPLANETARY;
        encounter.restore();
        encounter.restore();
        BOOST_TEST(groupsSaved == 1u);
        BOOST_TEST(groupsLoaded == 1u);
        BOOST_TEST((GLOBAL(CurrentActivity) & START_INTERPLANETARY) == 0);
    }
    BOOST_TEST(groupsLoaded == 1u);
}
BOOST_AUTO_TEST_CASE(orbital_encounter_scope_restores_on_early_return_but_preserves_load_abort_handoff)
{
    EncounterQueues queues;
    for (const auto flag : {ACTIVITY{0}, ACTIVITY{CHECK_LOAD}, ACTIVITY{CHECK_ABORT}}) {
        resetCalls();
        GLOBAL(CurrentActivity) = 0;
        {
            uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};
            GLOBAL(CurrentActivity) |= START_INTERPLANETARY | flag;
        }
        BOOST_TEST(groupsSaved == 1u);
        BOOST_TEST(groupsLoaded == (flag ? 0u : 1u));
        BOOST_TEST(bool(GLOBAL(CurrentActivity) & START_INTERPLANETARY) == bool(flag));
    }
}

// Dialogue/UI are simulated; use the real campaign reference provided by the mode.
COUNT uqm::EncounterRuntime::communicate(CONVERSATION conversation) {
    encounterCampaign = &story_;
    const auto result = InitCommunication(conversation);
    encounterCampaign = nullptr;
    return result;
}
