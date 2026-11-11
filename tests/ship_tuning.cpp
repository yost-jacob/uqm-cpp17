#include "uqm/ship_tuning.hpp"
#include <boost/test/unit_test.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <limits>
#include <stdexcept>

namespace {
using Json = nlohmann::json;
Json stock() {
    std::ifstream input(UQM_TEST_SHIPS_PATH);
    if (!input) throw std::runtime_error("cannot open stock ship tuning fixture");
    return Json::parse(input);
}
uqm::combat::ShipTuningCatalog parse(const Json &document) {
    std::istringstream input(document.dump());
    return uqm::combat::ShipTuningCatalog(input);
}
}
BOOST_AUTO_TEST_CASE(stock_ship_tuning_loads_complete_roster_and_representative_weapon_values)
{
    const auto catalog = parse(stock());
    BOOST_TEST(catalog.size() == 28u);
    BOOST_TEST(catalog.at("arilou").ship.maxCrew == 6u);
    BOOST_TEST(catalog.at("arilou").ship.maxThrust == 40u);
    BOOST_TEST(catalog.at("arilou").ship.thrustIncrement == 40u);
    BOOST_TEST(catalog.at("human").ship.maxCrew == 18u);
    BOOST_TEST(catalog.at("human").projectile("missile").speed == 40);
    BOOST_TEST(catalog.at("human").projectile("missile").damage == 4);
    BOOST_TEST(catalog.at("chenjesu").projectile("fragment").lifetime == 10u);
    BOOST_TEST(catalog.at("chenjesu").projectile("fragment").damage == 2);
    BOOST_TEST(catalog.at("probe").ship.maxThrust == 0u);
    BOOST_TEST(catalog.at("sis_ship").projectile("blaster").damage == 2);
    BOOST_CHECK_THROW(catalog.at("unknown"), std::out_of_range);
    BOOST_CHECK_THROW(catalog.at("human").projectile("unknown"), std::out_of_range);
}
BOOST_AUTO_TEST_CASE(ship_tuning_accepts_thousand_crew_and_owns_detached_immutable_values)
{
    auto document = stock();
    document["ships"]["arilou"]["max_crew"] = 1000;
    document["ships"]["human"]["projectiles"]["missile"]["damage"] = 12;
    const auto catalog = parse(document);
    BOOST_TEST(catalog.at("arilou").ship.maxCrew == 1000u);
    BOOST_TEST(catalog.at("human").projectile("missile").damage == 12);
    document["ships"]["arilou"]["max_crew"] = 3;
    BOOST_TEST(catalog.at("arilou").ship.maxCrew == 1000u);
    BOOST_TEST(&catalog.at("arilou") == &catalog.at("arilou"));
}
BOOST_AUTO_TEST_CASE(ship_tuning_rejects_nonintegers_and_narrowing_instead_of_coercing)
{
    for (const Json &value : {Json(0), Json(-1), Json(32768), Json(6.5), Json(true),
            Json("6"), Json(std::numeric_limits<std::uint64_t>::max())}) {
        auto document = stock();
        document["ships"]["arilou"]["max_crew"] = value;
        BOOST_CHECK_THROW(parse(document), std::runtime_error);
    }
    auto document = stock();
    document["ships"]["arilou"]["max_energy"] = 256;
    BOOST_CHECK_THROW(parse(document), std::runtime_error);
    document = stock();
    document["ships"]["arilou"]["weapon_wait"] = -1;
    BOOST_CHECK_THROW(parse(document), std::runtime_error);
}
BOOST_AUTO_TEST_CASE(ship_tuning_requires_exact_ship_and_projectile_field_sets)
{
    auto document = stock();
    document["ships"].erase("arilou");
    BOOST_CHECK_THROW(parse(document), std::runtime_error);
    document = stock();
    document["ships"]["arilou"]["max_creww"] = 1000;
    BOOST_CHECK_THROW(parse(document), std::runtime_error);
    document = stock();
    document["ships"]["human"]["projectiles"].erase("missile");
    BOOST_CHECK_THROW(parse(document), std::runtime_error);
    document = stock();
    document["ships"]["human"]["projectiles"]["missile"]["lifetime"] = 0;
    BOOST_CHECK_THROW(parse(document), std::runtime_error);
    document = stock();
    document["ships"]["human"]["projectiles"]["missile"]["damage"] = 256;
    BOOST_CHECK_THROW(parse(document), std::runtime_error);
    document = stock();
    document["schema_version"] = 3;
    BOOST_CHECK_THROW(parse(document), std::runtime_error);
}
BOOST_AUTO_TEST_CASE(flagship_tuning_preserves_representable_module_tiers)
{
    for (const auto damage : {0, 86}) {
        auto document = stock();
        document["ships"]["sis_ship"]["projectiles"]["blaster"]["damage"] = damage;
        BOOST_CHECK_THROW(parse(document), std::runtime_error);
    }
    auto document = stock();
    document["ships"]["sis_ship"]["projectiles"]["blaster"]["damage"] = 3;
    BOOST_TEST(parse(document).at("sis_ship").projectile("blaster").damage == 3);
}
BOOST_AUTO_TEST_CASE(ship_tuning_keeps_signed_regeneration_and_bounds_only_the_ai_range_hint)
{
    auto document = stock();
    document["ships"]["arilou"]["energy_regeneration"] = -1;
    BOOST_TEST(static_cast<std::int8_t>(parse(document).at("arilou").ship.energyRegeneration) == -1);
    BOOST_TEST(uqm::combat::weaponRangeHint(1200) == 1200u);
    BOOST_TEST(uqm::combat::weaponRangeHint(1000000) == 65535u);
    BOOST_TEST(uqm::combat::weaponRangeHint(-1) == 0u);
}

BOOST_AUTO_TEST_CASE(projectile_profiles_expose_shared_flight_and_special_weapon_stats)
{
    const auto catalog = parse(stock());
    const auto &human = catalog.at("human").projectile("missile");
    BOOST_TEST(human.flight.steeringPeriod == 4u);
    BOOST_TEST(human.flight.initialDelay == 3u);
    BOOST_TEST(human.flight.acceleration == 4);
    BOOST_TEST(human.flight.maxSpeed == 80);
    BOOST_TEST(human.flight.directionalSprite);
    BOOST_TEST(catalog.at("vux").projectile("limpet").flight.snapToTarget);
    BOOST_TEST(catalog.at("mycon").projectile("plasma").lifetime == 143u);
    BOOST_TEST(catalog.at("blackurq").projectile("gas").damage == 3);
    BOOST_TEST(catalog.at("melnorme").projectile("charge").speed == 180);
    BOOST_TEST(catalog.at("thradd").projectile("napalm").lifetime == 48u);
    BOOST_TEST(catalog.at("zoqfot").projectile("tongue").damage == 12);
}

BOOST_AUTO_TEST_CASE(projectile_flight_rejects_invalid_types_ranges_and_coupled_stats)
{
    for (const Json value : {Json(-1), Json(1024), Json(1.5), Json(true)}) {
        auto document = stock();
        document["ships"]["human"]["projectiles"]["missile"]["flight"]["maximum_speed"] = value;
        BOOST_CHECK_THROW(parse(document), std::runtime_error);
    }
    auto document = stock();
    document["ships"]["human"]["projectiles"]["missile"]["flight"]["directional_sprite"] = 1;
    BOOST_CHECK_THROW(parse(document), std::runtime_error);
    document = stock();
    document["ships"]["human"]["projectiles"]["missile"]["flight"].erase("initial_delay_ticks");
    BOOST_CHECK_THROW(parse(document), std::runtime_error);
    for (const auto *ship : {"mycon", "melnorme"}) {
        document = stock();
        document["ships"][ship]["projectiles"][std::string(ship) == "mycon" ? "plasma" : "charge"]["hit_points"] = 99;
        BOOST_CHECK_THROW(parse(document), std::runtime_error);
    }
}
