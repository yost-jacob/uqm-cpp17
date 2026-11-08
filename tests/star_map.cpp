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

static_assert(std::is_same_v<decltype(std::declval<StarMap &>().at(StarId{0})), const StarDesc &>);
static_assert(std::is_same_v<decltype(std::declval<const StarMap &>().find(
        std::declval<const StarSearch &>())), std::vector<StarId>>);

BOOST_AUTO_TEST_CASE(star_map_preserves_native_values_and_enums) {
    const auto map = load(source());
    BOOST_TEST(map->stars().size() == 518u);
    int solCount = 0, melnormeCount = 0;
    for (const auto &star : map->stars()) {
        BOOST_TEST(&map->at(star.id()) == &star);
        const auto &properties = star.properties();
        if (properties.specialSystem == SOL_DEFINED) {
            ++solCount;
            BOOST_TEST(star.name() == "Sol");
            BOOST_TEST(star.position().x == 1752);
            BOOST_TEST(star.position().y == 1450);
        }
        if (properties.specialSystem >= MELNORME0_DEFINED
                && properties.specialSystem <= MELNORME8_DEFINED)
            ++melnormeCount;
    }
    BOOST_TEST(solCount == 1);
    BOOST_TEST(melnormeCount == 9);
    BOOST_TEST(map->at(StarId{0}).name() == "Zeta Serpentis");
    BOOST_TEST(map->at(StarId{501}).name() == "Microscopii");
    BOOST_CHECK_THROW(map->at(StarId{502}), std::out_of_range);
    BOOST_CHECK_THROW(map->at(StarId{519}), std::out_of_range);
    BOOST_CHECK_THROW(map->at(StarId{99999}), std::out_of_range);
}

BOOST_AUTO_TEST_CASE(star_map_owns_editable_utf8_text_without_a_game_string_catalogue) {
    auto document = source();
    document["hyperspace"][0]["prefix"] = "Mega";
    document["hyperspace"][0]["postfix"] = "Butt Mountain °";
    const auto map = load(document);
    document.clear();
    BOOST_TEST(map->at(StarId{0}).prefix() == "Mega");
    BOOST_TEST(map->at(StarId{0}).postfix() == "Butt Mountain °");
    BOOST_TEST(map->at(StarId{0}).name() == "Mega Butt Mountain °");
    BOOST_TEST(map->at(StarId{0}).properties().postfixId == 74u);
    BOOST_TEST(map->at(StarId{1}).name() == "Eta Giclas");
}

BOOST_AUTO_TEST_CASE(star_map_search_returns_owned_ids_with_typed_space_and_bounds) {
    auto map = load(source());
    for (const auto &star : map->stars()) {
        const auto matches = map->find({star.space(), star.position(), 0, 0});
        BOOST_REQUIRE(matches.size() == 1u);
        BOOST_TEST(matches.front().value == star.id().value);
    }
    const auto hyper = map->find({StarSpace::HyperSpace, {0, 0}, std::nullopt, std::nullopt});
    BOOST_TEST(hyper.size() == 502u);
    for (size_t i = 0; i < hyper.size(); ++i)
        BOOST_TEST(hyper[i].value == i);
    const auto quasi = map->find({StarSpace::QuasiSpace, {0, 0}, std::nullopt, std::nullopt});
    BOOST_TEST(quasi.size() == 16u);
    for (size_t i = 0; i < quasi.size(); ++i)
        BOOST_TEST(quasi[i].value == 503 + i);
    auto edge = map->at(StarId{0}).position();
    edge.x -= 5;
    BOOST_TEST(map->find({StarSpace::HyperSpace, edge, 5, 0}).front().value == 0u);
    BOOST_TEST(map->find({StarSpace::HyperSpace, edge, 4, 0}).empty());
    BOOST_CHECK_THROW(map->find({StarSpace::HyperSpace, edge, -1, 0}), std::invalid_argument);
    BOOST_CHECK_THROW(map->find({StarSpace::HyperSpace, edge, 0, -1}), std::invalid_argument);
    BOOST_TEST(map->find({StarSpace::HyperSpace, {INT32_MIN, INT32_MAX}, 1, 1}).empty());
    map.reset(); // IDs in a returned vector never point into the map.
    BOOST_TEST(hyper.front().value == 0u);
    BOOST_TEST(quasi.back().value == 518u);
}

BOOST_AUTO_TEST_CASE(star_map_coordinate_edits_do_not_require_reordering_saved_identities) {
    auto document = source();
    document["hyperspace"][0]["x"] = 9999;
    document["hyperspace"][0]["y"] = 9999;
    const auto map = load(document);
    const auto matches = map->find({StarSpace::HyperSpace, {9999, 9999}, 0, 0});
    BOOST_REQUIRE(matches.size() == 1u);
    BOOST_TEST(matches.front().value == 0u);
    BOOST_TEST(map->at(StarId{1}).id().value == 1u);
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
