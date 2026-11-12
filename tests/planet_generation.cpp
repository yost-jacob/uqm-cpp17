#define BOOST_TEST_MODULE uqm_planet_generation
#include <boost/test/unit_test.hpp>
#include "uqm/planets/planets.h"
#include <memory>
#include <array>
extern "C" const PlanetFrame planet_array[];
namespace {
PlanetAnalysisInput inputs(PlanetAnalysisPolicy policy, SIZE radius = EARTH_RADIUS) {
    return {MAKE_STAR(DWARF_STAR, YELLOW_BODY, -1), radius, planet_array, policy};
}
void compare(const PLANET_INFO &a, const PLANET_INFO &b) {
    BOOST_TEST(a.SurfaceTemperature == b.SurfaceTemperature);
    BOOST_TEST(a.AtmoDensity == b.AtmoDensity);
    BOOST_TEST(a.PlanetRadius == b.PlanetRadius);
    BOOST_TEST(a.SurfaceGravity == b.SurfaceGravity);
    BOOST_TEST(a.RotationPeriod == b.RotationPeriod);
    BOOST_TEST(a.AxialTilt == b.AxialTilt);
    BOOST_TEST(a.Weather == b.Weather);
    BOOST_TEST(a.Tectonics == b.Tectonics);
    BOOST_TEST(a.LifeChance == b.LifeChance);
}
}
BOOST_AUTO_TEST_CASE(analysis_is_independent_of_output_address_and_other_streams)
{
    for (BYTE type : {BYTE{DUST_WORLD}, BYTE{PRIMORDIAL_WORLD}, BYTE{WATER_WORLD}, BYTE{BLU_GAS_GIANT}}) {
        PLANET_DESC planet{};
        planet.rand_seed = 123456;
        planet.data_index = type;
        const auto configuration = inputs(PLANET_ANALYSIS_ORBIT);
        SYSTEM_INFO stack{};
        auto heap = std::make_unique<SYSTEM_INFO>();
        RandomContext first{RandomDefaultSeed}, second{RandomDefaultSeed}, unrelated{987};
        DoPlanetaryAnalysis(&stack, &planet, &first, &configuration);
        for (unsigned i = 0; i < 100; ++i) RandomContext_Random(&unrelated);
        DoPlanetaryAnalysis(heap.get(), &planet, &second, &configuration);
        compare(stack.PlanetInfo, heap->PlanetInfo);
        BOOST_TEST(first.seed == second.seed);
    }
}
BOOST_AUTO_TEST_CASE(preview_and_orbit_analysis_have_explicit_temperature_policy)
{
    PLANET_DESC planet{};
    planet.rand_seed = 123456;
    planet.data_index = PRIMORDIAL_WORLD;
    auto previewInput = inputs(PLANET_ANALYSIS_PREVIEW, EARTH_RADIUS / 10);
    auto orbitInput = previewInput;
    orbitInput.policy = PLANET_ANALYSIS_ORBIT;
    SYSTEM_INFO preview{}, orbit{};
    RandomContext first{RandomDefaultSeed}, second{RandomDefaultSeed};
    DoPlanetaryAnalysis(&preview, &planet, &first, &previewInput);
    DoPlanetaryAnalysis(&orbit, &planet, &second, &orbitInput);
    BOOST_TEST(orbit.PlanetInfo.SurfaceTemperature > preview.PlanetInfo.SurfaceTemperature);
    BOOST_TEST(first.seed != second.seed); // Preserve the policy's extra draws.
}
BOOST_AUTO_TEST_CASE(scan_generation_reproduces_node_identity_and_stream_position)
{
    PLANET_DESC planet{};
    planet.data_index = WATER_WORLD;
    planet.rand_seed = 123456;
    SYSTEM_INFO info{};
    const auto configuration = inputs(PLANET_ANALYSIS_ORBIT);
    RandomContext generation{RandomDefaultSeed};
    DoPlanetaryAnalysis(&info, &planet, &generation, &configuration);
    info.PlanetInfo.ScanSeed[BIOLOGICAL_SCAN] = generation.seed;
    GenerateLifeForms(&info, GENERATE_ALL, nullptr, &generation, EXTENT{210, 67});
    info.PlanetInfo.ScanSeed[MINERAL_SCAN] = generation.seed;
    const auto minerals = GenerateMineralDeposits(&info, GENERATE_ALL, nullptr, &generation, EXTENT{210, 67});
    BOOST_TEST(minerals > 0u);
    BOOST_TEST(minerals <= 32u);
    for (COUNT node = 0; node < minerals; ++node) {
        NODE_INFO first{}, second{};
        RandomContext a{1}, b{987654};
        GenerateMineralDeposits(&info, node, &first, &a, EXTENT{210, 67});
        GenerateMineralDeposits(&info, node, &second, &b, EXTENT{210, 67});
        BOOST_TEST(first.type == second.type);
        BOOST_TEST(first.density == second.density);
        BOOST_TEST(first.loc_pt.x == second.loc_pt.x);
        BOOST_TEST(first.loc_pt.y == second.loc_pt.y);
        BOOST_TEST(a.seed == b.seed);
    }
    RandomContext a{1}, b{2};
    std::array<NODE_INFO, 2> nodes{};
    GenerateRandomNodes(&info, MINERAL_SCAN, 5, 7, 3, &nodes[0], &a, EXTENT{210, 67});
    GenerateRandomNodes(&info, MINERAL_SCAN, 5, 7, 3, &nodes[1], &b, EXTENT{210, 67});
    BOOST_TEST(nodes[0].loc_pt.x == nodes[1].loc_pt.x);
    BOOST_TEST(nodes[0].loc_pt.y == nodes[1].loc_pt.y);
    BOOST_TEST(a.seed == b.seed);
}

BOOST_AUTO_TEST_CASE(biological_node_identity_uses_explicit_bounds_and_independent_workspace)
{
    SYSTEM_INFO info{};
    info.PlanetInfo.PlanDataPtr = &planet_array[WATER_WORLD];
    info.PlanetInfo.LifeChance = 1024;
    info.PlanetInfo.ScanSeed[BIOLOGICAL_SCAN] = 12345;
    RandomContext generation{RandomDefaultSeed};
    const EXTENT bounds{210, 67};
    const auto count = GenerateLifeForms(&info, GENERATE_ALL, nullptr, &generation, bounds);
    BOOST_TEST(count > 0u);
    for (COUNT node = 0; node < count; ++node) {
        NODE_INFO a{}, b{};
        RandomContext first{1}, second{999};
        GenerateLifeForms(&info, node, &a, &first, bounds);
        GenerateLifeForms(&info, node, &b, &second, bounds);
        BOOST_TEST(a.type == b.type);
        BOOST_TEST(a.loc_pt.x == b.loc_pt.x);
        BOOST_TEST(a.loc_pt.y == b.loc_pt.y);
        BOOST_TEST(first.seed == second.seed);
        BOOST_TEST(a.loc_pt.x >= 8); BOOST_TEST(a.loc_pt.x < bounds.width - 8);
        BOOST_TEST(a.loc_pt.y >= 8); BOOST_TEST(a.loc_pt.y < bounds.height - 8);
    }
}
