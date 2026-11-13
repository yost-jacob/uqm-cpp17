#include "uqm/ship_factory.hpp"
#include "uqm/ship_builders.hpp"
#include "uqm/build.h"
#include "uqm/init.h"
#include "uqm/game_state.hpp"
#include "libs/strings/strintrn.h"
#include <map>
#include <string>
#include <boost/test/unit_test.hpp>
#include <stdexcept>
#include <set>
#include <cstddef>

bool allowTestLanderSound = false;
std::string failedTestGraphic;
// Replace concrete behavior construction, not the factory under test. Each
// builder marks its own identity so a crossed/missing switch arm is observable.
namespace {
unsigned liveShips = 0;
bool failConstruction = false;
bool assetFixture = false;
bool failAnimation = false;
bool battleSuccess = false;
std::map<std::string, unsigned> graphicLoads;
std::set<MUSIC_REF> musicHandles;
unsigned soundHandles = 0;
unsigned completions = 0;
struct alignas(std::max_align_t) FakeImage {};
std::set<void *> images;
class TaggedShip final : public uqm::combat::Ship {
public:
    explicit TaggedShip(SPECIES_ID id) : Ship(definition(id)) { ++liveShips; }
    ~TaggedShip() override { --liveShips; }
private:
    static RACE_DESC definition(SPECIES_ID id) {
        RACE_DESC result{};
        result.ship_info.ship_cost = static_cast<BYTE>(id);
        if (assetFixture) {
            result.ship_info.icons_rsc = "test.icons";
            result.ship_info.melee_icon_rsc = "test.melee";
            result.ship_data.ship_rsc[0] = "test.ship";
            result.ship_data.ship_rsc[1] = "test.ship.medium";
            result.ship_data.weapon_rsc[0] = "test.weapon";
            result.ship_data.special_rsc[0] = "test.special";
            result.ship_data.captain_control.captain_rsc = "test.captain";
            result.ship_data.victory_ditty_rsc = "test.music.failure";
            result.ship_data.ship_sounds_rsc = "test.sounds";
        }
        return result;
    }
    COUNT createWeapons(ELEMENT &, uqm::combat::WeaponSlots &) noexcept override { return 0; }
    void onFinish() noexcept override { ++completions; }
};
std::unique_ptr<uqm::combat::Ship> tagged(SPECIES_ID id) {
    if (failConstruction) throw std::runtime_error("builder failed");
    return std::make_unique<TaggedShip>(id);
}
}
namespace uqm::combat {
std::unique_ptr<Ship> makeArilouShip() { return tagged(ARILOU_ID); }
std::unique_ptr<Ship> makeChmmrShip() { return tagged(CHMMR_ID); }
std::unique_ptr<Ship> makeEarthlingShip() { return tagged(EARTHLING_ID); }
std::unique_ptr<Ship> makeOrzShip() { return tagged(ORZ_ID); }
std::unique_ptr<Ship> makePkunkShip() { return tagged(PKUNK_ID); }
std::unique_ptr<Ship> makeShofixtiShip() { return tagged(SHOFIXTI_ID); }
std::unique_ptr<Ship> makeSpathiShip() { return tagged(SPATHI_ID); }
std::unique_ptr<Ship> makeSupoxShip() { return tagged(SUPOX_ID); }
std::unique_ptr<Ship> makeThraddashShip() { return tagged(THRADDASH_ID); }
std::unique_ptr<Ship> makeUtwigShip() { return tagged(UTWIG_ID); }
std::unique_ptr<Ship> makeVuxShip() { return tagged(VUX_ID); }
std::unique_ptr<Ship> makeYehatShip() { return tagged(YEHAT_ID); }
std::unique_ptr<Ship> makeMelnormeShip() { return tagged(MELNORME_ID); }
std::unique_ptr<Ship> makeDruugeShip() { return tagged(DRUUGE_ID); }
std::unique_ptr<Ship> makeIlwrathShip() { return tagged(ILWRATH_ID); }
std::unique_ptr<Ship> makeMyconShip() { return tagged(MYCON_ID); }
std::unique_ptr<Ship> makeSlylandroShip() { return tagged(SLYLANDRO_ID); }
std::unique_ptr<Ship> makeUmgahShip() { return tagged(UMGAH_ID); }
std::unique_ptr<Ship> makeUrQuanShip() { return tagged(UR_QUAN_ID); }
std::unique_ptr<Ship> makeZoqFotPikShip() { return tagged(ZOQFOTPIK_ID); }
std::unique_ptr<Ship> makeSyreenShip() { return tagged(SYREEN_ID); }
std::unique_ptr<Ship> makeKohrAhShip() { return tagged(KOHR_AH_ID); }
std::unique_ptr<Ship> makeAndrosynthShip() { return tagged(ANDROSYNTH_ID); }
std::unique_ptr<Ship> makeChenjesuShip() { return tagged(CHENJESU_ID); }
std::unique_ptr<Ship> makeMmrnmhrmShip() { return tagged(MMRNMHRM_ID); }
std::unique_ptr<Ship> makeFlagship() { return tagged(SIS_SHIP_ID); }
std::unique_ptr<Ship> makeSaMatraShip() { return tagged(SA_MATRA_ID); }
std::unique_ptr<Ship> makeUrQuanProbeShip() { return tagged(UR_QUAN_PROBE_ID); }
}
BOOST_AUTO_TEST_CASE(direct_ship_factory_preserves_every_species_mapping_and_unique_ownership)
{
    BOOST_TEST(liveShips == 0u);
    for (unsigned id = ARILOU_ID; id < NUM_SPECIES_ID; ++id) {
        auto ship = uqm::combat::createShip(static_cast<SPECIES_ID>(id));
        BOOST_REQUIRE(ship);
        BOOST_TEST(ship->legacyData().ship_info.ship_cost == id);
        BOOST_TEST(ship->legacyData().ship_owner == ship.get());
        BOOST_TEST(liveShips == 1u);
    }
    BOOST_TEST(liveShips == 0u);
}
BOOST_AUTO_TEST_CASE(direct_ship_factory_rejects_empty_and_unsupported_species_without_construction)
{
    BOOST_TEST(!uqm::combat::createShip(NO_ID));
    BOOST_TEST(!uqm::combat::createShip(NUM_SPECIES_ID));
    // 31 lies inside this legacy enum's representable range but has no behavior.
    BOOST_TEST(!uqm::combat::createShip(static_cast<SPECIES_ID>(31)));
    BOOST_TEST(liveShips == 0u);
}
BOOST_AUTO_TEST_CASE(native_factory_leaves_exception_translation_to_the_c_load_boundary)
{
    failConstruction = true;
    BOOST_CHECK_THROW(uqm::combat::createShip(ARILOU_ID), std::runtime_error);
    failConstruction = false;
    BOOST_TEST(liveShips == 0u);
    BOOST_REQUIRE(uqm::combat::createShip(ARILOU_ID));
    BOOST_TEST(liveShips == 0u);
}

namespace {
struct AssetFixture {
    AssetFixture() {
        BOOST_REQUIRE(images.empty());
        BOOST_REQUIRE(liveShips == 0u);
        assetFixture = true;
        completions = 0;
        graphicLoads.clear();
    }
    ~AssetFixture() {
        assetFixture = false;
        failAnimation = false;
        battleSuccess = false;
        failConstruction = false;
        BOOST_TEST(images.empty());
        BOOST_TEST(musicHandles.empty());
        BOOST_TEST(soundHandles == 0u);
        BOOST_TEST(liveShips == 0u);
        for (auto *image : images) delete static_cast<FakeImage *>(image);
        images.clear();
    }
};
}
// Minimal resource backend for the loader ownership tests. Opaque graphics are
// never dereferenced; string-table functions use the real existing test linkage.
extern "C" void *LoadGraphicInstance(RESOURCE resource) {
    if (!failedTestGraphic.empty() && failedTestGraphic == resource) return nullptr;
    if (std::string(resource) == "font.lander") return new char;
    if (failAnimation && std::string(resource) == "test.ship.medium") return nullptr;
    ++graphicLoads[resource];
    auto *image = new FakeImage;
    images.insert(image);
    return image;
}
std::size_t liveTestDrawables() noexcept { return images.size(); }
extern "C" FRAME CaptureDrawable(DRAWABLE drawable) { return reinterpret_cast<FRAME>(drawable); }
extern "C" DRAWABLE ReleaseDrawable(FRAME frame) { return reinterpret_cast<DRAWABLE>(frame); }
extern "C" BOOLEAN DestroyDrawable(DRAWABLE drawable) {
    if (!drawable) return TRUE;
    auto *image = reinterpret_cast<FakeImage *>(drawable);
    const bool owned = images.erase(image) == 1u;
    BOOST_TEST(owned);
    if (!owned) return FALSE;
    delete image;
    return TRUE;
}
namespace { std::string lastMusicResource; }
std::string lastLoadedTestMusicResource() { return lastMusicResource; }
extern "C" MUSIC_REF LoadMusicInstance(RESOURCE resource) {
    lastMusicResource = resource;
    if (!battleSuccess && lastMusicResource.rfind("music.orbit", 0) != 0) return nullptr; // Required late-stage failure fixture.
    auto handle = reinterpret_cast<MUSIC_REF>(new FakeImage);
    musicHandles.insert(handle);
    return handle;
}
extern "C" SOUND_REF LoadSoundInstance(RESOURCE resource) { if (!battleSuccess && !(allowTestLanderSound && std::string(resource) == "sounds.lander")) return nullptr; ++soundHandles; return AllocStringTable(1); }
extern "C" BOOLEAN DestroyMusic(MUSIC_REF music) { if (music) { BOOST_TEST(musicHandles.erase(music) == 1u); delete reinterpret_cast<FakeImage *>(music); } return TRUE; }
extern "C" BOOLEAN DestroySound(SOUND_REF sound) { if (sound) { --soundHandles; DestroyStringTable(sound); } return TRUE; }

BOOST_FIXTURE_TEST_CASE(ship_load_failure_rolls_back_partial_animation_without_publishing_state, AssetFixture)
{
    failAnimation = true;
    BOOST_TEST(load_ship(ARILOU_ID, TRUE) == nullptr);
    BOOST_TEST(images.empty());
    BOOST_TEST(completions == 0u);
}
BOOST_FIXTURE_TEST_CASE(ship_load_failure_rolls_back_all_previously_loaded_asset_groups, AssetFixture)
{
    // Music fails after icons, all three animation groups and the captain frame.
    BOOST_TEST(load_ship(ARILOU_ID, TRUE) == nullptr);
    BOOST_TEST(images.empty());
    BOOST_TEST(completions == 0u);
}
BOOST_FIXTURE_TEST_CASE(ship_load_boundary_catches_native_construction_errors, AssetFixture)
{
    failConstruction = true;
    BOOST_TEST(load_ship(ARILOU_ID, FALSE) == nullptr);
    BOOST_TEST(images.empty());
    BOOST_TEST(completions == 0u);
}
BOOST_FIXTURE_TEST_CASE(ship_release_preserves_explicit_icon_transfer_to_legacy_callers, AssetFixture)
{
    auto *descriptor = load_ship(ARILOU_ID, FALSE);
    BOOST_REQUIRE(descriptor);
    const auto icon = descriptor->ship_info.icons;
    const auto melee = descriptor->ship_info.melee_icon;
    BOOST_TEST(images.size() == 2u);
    free_ship(descriptor, FALSE); // Master ship list now owns these icons.
    BOOST_TEST(images.size() == 2u);
    BOOST_TEST(completions == 1u);
    BOOST_TEST(liveShips == 0u);
    DestroyDrawable(ReleaseDrawable(icon));
    DestroyDrawable(ReleaseDrawable(melee));
    free_ship(nullptr, TRUE);
}

BOOST_FIXTURE_TEST_CASE(repeated_ships_share_battle_assets_but_not_gameplay_or_transferred_icons, AssetFixture)
{
    battleSuccess = true;
    auto &cache = uqm::gameSession().assets();
    BOOST_TEST(cache.size() == 0u);
    auto *first = load_ship(ARILOU_ID, TRUE);
    BOOST_REQUIRE(first);
    const auto once = images.size();
    auto *second = load_ship(ARILOU_ID, TRUE);
    BOOST_REQUIRE(second);
    BOOST_TEST(images.size() == once + 2u); // Only separately transferred icons duplicate.
    BOOST_TEST(first->ship_data.ship[0] == second->ship_data.ship[0]);
    BOOST_TEST(first->ship_data.ship[1] == second->ship_data.ship[2]); // Small view aliases medium.
    BOOST_TEST(first->ship_data.captain_control.background == second->ship_data.captain_control.background);
    BOOST_TEST(first->ship_data.victory_ditty == second->ship_data.victory_ditty);
    BOOST_TEST(first->ship_data.ship_sounds == second->ship_data.ship_sounds);
    first->ship_info.crew_level = 17;
    BOOST_TEST(second->ship_info.crew_level == 0u);
    BOOST_TEST(graphicLoads["test.ship"] == 1u);
    BOOST_TEST(musicHandles.size() == 1u);
    BOOST_TEST(soundHandles == 1u);
    free_ship(first, TRUE);
    BOOST_TEST(cache.size() != 0u);
    BOOST_TEST(graphicLoads["test.ship"] == 1u);
    free_ship(second, TRUE);
    BOOST_TEST(cache.size() == 0u);
    BOOST_TEST(images.empty());
    BOOST_TEST(musicHandles.empty());
    BOOST_TEST(soundHandles == 0u);
}
