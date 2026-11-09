#include "uqm/ship_factory.hpp"
#include "uqm/ship_builders.hpp"
#include "uqm/build.h"
#include "uqm/init.h"
#include <boost/test/unit_test.hpp>
#include <stdexcept>
#include <set>
#include <cstddef>

// Replace concrete behavior construction, not the factory under test. Each
// builder marks its own identity so a crossed/missing switch arm is observable.
namespace {
unsigned liveShips = 0;
bool failConstruction = false;
bool assetFixture = false;
bool failAnimation = false;
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
            result.ship_data.weapon_rsc[0] = "test.weapon";
            result.ship_data.special_rsc[0] = "test.special";
            result.ship_data.captain_control.captain_rsc = "test.captain";
            result.ship_data.victory_ditty_rsc = "test.music.failure";
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
    }
    ~AssetFixture() {
        assetFixture = false;
        failAnimation = false;
        failConstruction = false;
        BOOST_TEST(images.empty());
        BOOST_TEST(liveShips == 0u);
        for (auto *image : images) delete static_cast<FakeImage *>(image);
        images.clear();
    }
};
}
// Minimal resource backend for the loader ownership tests. Opaque graphics are
// never dereferenced; string-table functions use the real existing test linkage.
extern "C" void *LoadGraphicInstance(RESOURCE) {
    auto *image = new FakeImage;
    images.insert(image);
    return image;
}
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
extern "C" BOOLEAN load_animation(FRAME *frames, RESOURCE big, RESOURCE, RESOURCE) {
    frames[0] = CaptureDrawable(static_cast<DRAWABLE>(LoadGraphicInstance(big)));
    if (failAnimation) return FALSE; // Partial group, as a missing medium view would leave it.
    frames[1] = frames[2] = frames[0]; // The real backend permits aliased views.
    return TRUE;
}
extern "C" BOOLEAN free_image(FRAME *frames) {
    std::set<FRAME> unique;
    for (unsigned i = 0; i < NUM_VIEWS; ++i) {
        if (frames[i]) unique.insert(frames[i]);
        frames[i] = nullptr;
    }
    for (auto frame : unique) DestroyDrawable(ReleaseDrawable(frame));
    return TRUE;
}
extern "C" MUSIC_REF LoadMusicInstance(RESOURCE) { return nullptr; } // Required late-stage failure.
extern "C" SOUND_REF LoadSoundInstance(RESOURCE) { return nullptr; }
extern "C" BOOLEAN DestroyMusic(MUSIC_REF music) { BOOST_TEST(music == nullptr); return TRUE; }
extern "C" BOOLEAN DestroySound(SOUND_REF sound) { BOOST_TEST(sound == nullptr); return TRUE; }

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
    free_ship(descriptor, FALSE, FALSE); // Master ship list now owns these icons.
    BOOST_TEST(images.size() == 2u);
    BOOST_TEST(completions == 1u);
    BOOST_TEST(liveShips == 0u);
    DestroyDrawable(ReleaseDrawable(icon));
    DestroyDrawable(ReleaseDrawable(melee));
    free_ship(nullptr, TRUE, TRUE);
}
