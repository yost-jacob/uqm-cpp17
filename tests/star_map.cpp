#include <boost/test/unit_test.hpp>
#include <fstream>
#include <memory>
#include <sstream>
#include <type_traits>
#include <nlohmann/json.hpp>
#include "uqm/star_map.hpp"
#include "uqm/gendef.h"

namespace {
using Json = nlohmann::json;
Json source() {
    std::ifstream input(UQM_TEST_STARMAP_PATH);
    if (!input)
        throw std::runtime_error("cannot open test star catalogue");
    return Json::parse(input);
}
std::unique_ptr<StarMap> load(const Json &document) {
    std::istringstream input(document.dump());
    return std::make_unique<StarMap>(input);
}
}
static_assert(!std::is_copy_constructible_v<StarMap>);

BOOST_AUTO_TEST_CASE(star_map_preserves_entire_legacy_catalogue_and_enums) {
    const auto map = load(source());
    BOOST_TEST(map->size() == STAR_CATALOGUE_SIZE);
    // Golden FNV1a of all 4,160 bytes in the original compiled starmap_array.
    // Encode the same eight save fields; the new in-memory identity is excluded.
    uint64_t hash = UINT64_C(14695981039346656037);
    int solCount = 0, melnormeCount = 0;
    for (size_t i = 0; i < map->size(); ++i) {
        const auto &star = map->at(i).descriptor();
        BOOST_TEST(map->indexOf(star) == i);
        const unsigned bytes[]{unsigned(star.star_pt.x) & 255, unsigned(star.star_pt.x) >> 8,
            unsigned(star.star_pt.y) & 255, unsigned(star.star_pt.y) >> 8,
            star.Type, star.Index, star.Prefix, star.Postfix};
        for (const auto byte : bytes)
            hash = (hash ^ byte) * UINT64_C(1099511628211);
        if (star.Index == SOL_DEFINED) {
            ++solCount;
            BOOST_TEST(map->at(i).name() == "Sol");
            BOOST_TEST(star.star_pt.x == 1752);
            BOOST_TEST(star.star_pt.y == 1450);
        }
        if (star.Index >= MELNORME0_DEFINED && star.Index <= MELNORME8_DEFINED)
            ++melnormeCount;
    }
    BOOST_TEST(hash == UINT64_C(0xb815f9d5b77cef91));
    BOOST_TEST(solCount == 1);
    BOOST_TEST(melnormeCount == 9);
    BOOST_TEST(map->at(0).name() == "Zeta Serpentis");
    BOOST_TEST(map->at(501).name() == "Microscopii");
    BOOST_TEST(map->at(502).descriptor().star_pt.x == 19998);
    BOOST_TEST(map->at(519).descriptor().star_pt.y == 19998);
}

BOOST_AUTO_TEST_CASE(star_map_owns_editable_utf8_text_without_a_game_string_catalogue) {
    auto document = source();
    document["hyperspace"][0]["prefix"] = "Mega";
    document["hyperspace"][0]["postfix"] = "Butt Mountain °";
    const auto map = load(document);
    document.clear(); // No JSON or shared text storage is borrowed by the stars.
    BOOST_TEST(map->at(0).prefix() == "Mega");
    BOOST_TEST(map->at(0).postfix() == "Butt Mountain °");
    BOOST_TEST(map->at(0).name() == "Mega Butt Mountain °");
    BOOST_TEST(map->at(0).descriptor().Postfix == 74u);
    BOOST_TEST(map->at(1).name() == "Eta Giclas");
}

BOOST_AUTO_TEST_CASE(star_map_search_preserves_indices_spaces_and_inclusive_bounds) {
    const auto map = load(source());
    for (size_t i = 0; i < map->size(); ++i) {
        if (i == NUM_SOLAR_SYSTEMS || i == STAR_CATALOGUE_SIZE - 1)
            continue;
        auto &star = map->at(i).descriptor();
        BOOST_TEST(map->find(nullptr, star.star_pt, 0, 0, i >= QUASISPACE_FIRST) == &star);
    }
    const POINT anywhere{0, 0};
    const STAR_DESC *last = nullptr;
    size_t count = 0;
    while ((last = map->find(last, anywhere, -1, -1, false))) {
        BOOST_TEST(map->indexOf(*last) == count);
        ++count;
    }
    BOOST_TEST(count == NUM_SOLAR_SYSTEMS);
    count = 0;
    while ((last = map->find(last, anywhere, -1, -1, true))) {
        BOOST_TEST(map->indexOf(*last) == QUASISPACE_FIRST + count);
        ++count;
    }
    BOOST_TEST(count == NUM_QUASISPACE_LOCATIONS);
    POINT edge = map->at(0).descriptor().star_pt;
    edge.x -= 5;
    BOOST_TEST(map->find(nullptr, edge, 5, 0, false) == &map->at(0).descriptor());
    BOOST_TEST(map->find(nullptr, edge, 4, 0, false) == nullptr);
}

BOOST_AUTO_TEST_CASE(star_map_coordinate_edits_do_not_require_reordering_saved_identities) {
    auto document = source();
    document["hyperspace"][0]["x"] = 9999;
    document["hyperspace"][0]["y"] = 9999;
    const auto map = load(document);
    const POINT position{9999, 9999};
    BOOST_TEST(map->find(nullptr, position, 0, 0, false) == &map->at(0).descriptor());
    BOOST_TEST(map->indexOf(map->at(1).descriptor()) == 1u);
}

BOOST_AUTO_TEST_CASE(star_map_rejects_invalid_records_and_preserves_index_contracts) {
    const auto original = source();
    for (const auto &change : {Json{{"x", -1}}, Json{{"y", 10000}},
            Json{{"x", 10.5}}, Json{{"type", "UNKNOWN_STAR"}},
            Json{{"special_system", 256}}, Json{{"catalogue_index", 1}},
            Json{{"prefix_id", 15}}, Json{{"postfix", ""}},
            Json{{"postfix", std::string("a\0b", 3)}},
            Json{{"postfix", std::string(256, 'x')}}}) {
        auto document = original;
        document["hyperspace"][0].update(change);
        BOOST_CHECK_THROW(load(document), std::exception);
    }
    auto document = original;
    document["quasispace"].erase(0);
    BOOST_CHECK_THROW(load(document), std::exception);
    document = original;
    document["schema_version"] = 2;
    BOOST_CHECK_THROW(load(document), std::exception);
    std::istringstream malformed("{oops}");
    BOOST_CHECK_THROW(StarMap{malformed}, std::exception);
}
