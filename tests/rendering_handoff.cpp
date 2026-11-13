#include "uqm/system_runtime.hpp"
#include "uqm/exploration_rendering.hpp"
#include "uqm/controls.h"
#include "uqm/orbit_presentation.hpp"
#include "uqm/planets/scan.h"
#include "uqm/settings.h"
#include "uqm/setup.h"
#include "libs/graphics/context.h"
#include "libs/graphics/gfx_common.h"
#include <boost/test/unit_test.hpp>
#include <array>
#include <utility>

namespace {
CONTEXT active{};
std::array<context_desc, 4> contexts{};
int scale = 256, scaleMode = 1;
POINT origin{3, 4};
void parentFrame() {}
void childFrame() {}
InputFrameCallback *callback = parentFrame;
bool failContext = false;
}
// Fake backend state; the production RAII capture/restore implementation is real.
extern "C" {
CONTEXT SpaceContext{}, RadarContext{};
FRAME Screen = nullptr;
#ifdef DEBUG
CONTEXT CreateContextAux(const char *) { if (std::exchange(failContext, false)) return nullptr; return new context_desc{}; }
#else
CONTEXT CreateContextAux() { if (std::exchange(failContext, false)) return nullptr; return new context_desc{}; }
#endif
BOOLEAN GetContextClipRect(RECT *rect) { *rect = active ? active->ClipRect : RECT{{0, 0}, {320, 240}}; return TRUE; }
BOOLEAN PLRPlaying(MUSIC_REF) { return FALSE; }
void PlayMusic(MUSIC_REF, BOOLEAN, BYTE) {}
CONTEXT GetContext() { return active; }
CONTEXT SetContext(CONTEXT value) { auto previous = active; active = value; return previous; }
FRAME SetContextFGFrame(FRAME value) { auto previous = active->ForeGroundFrame; active->ForeGroundFrame = value; return previous; }
FONT SetContextFont(FONT value) { auto previous = active->Font; active->Font = value; return previous; }
FRAME SetContextFontEffect(FRAME value) { auto previous = active->FontEffect; active->FontEffect = value; return previous; }
Color SetContextForeGroundColor(Color value) { auto previous = active->ForeGroundColor; active->ForeGroundColor = value; return previous; }
Color SetContextBackGroundColor(Color value) { auto previous = active->BackGroundColor; active->BackGroundColor = value; return previous; }
DrawMode SetContextDrawMode(DrawMode value) { auto previous = active->Mode; active->Mode = value; return previous; }
BOOLEAN SetContextClipRect(RECT *value) { active->ClipRect = value ? *value : RECT{}; return TRUE; }
POINT GetFrameHot(FRAME) { return origin; }
POINT SetContextOrigin(POINT value) { auto previous = origin; origin = value; return previous; }
InputFrameCallback *SetInputCallback(InputFrameCallback *value) { auto previous = callback; callback = value; return previous; }
int GetGraphicScale() { return scale; }
int SetGraphicScale(int value) { auto previous = scale; scale = value; return previous; }
int GetGraphicScaleMode() { return scaleMode; }
int SetGraphicScaleMode(int value) { auto previous = scaleMode; scaleMode = value; return previous; }
}
namespace uqm { void redrawExploration(SOLARSYS_STATE &, ExplorationView) {} }

BOOST_AUTO_TEST_CASE(renderer_handoff_restores_parent_bindings_and_rotation_after_child_changes)
{
    SpaceContext = &contexts[1]; RadarContext = &contexts[2];
    uqm::GameSession session{std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Pilot"}), uqm::SinglePlayerId};
    SOLARSYS_STATE system{};
    uqm::OrbitPresentation presentation{session, system};
    std::array<CONTEXT, 4> handles{presentation.planetContext(), SpaceContext, RadarContext, presentation.scanContext()};
    active = &contexts[3]; callback = parentFrame; scale = 256; scaleMode = 1; origin = {3, 4};
    alignas(std::max_align_t) static char token;
    for (auto *handle : handles) {
        auto &context = *handle;
        context = {};
        context.ForeGroundFrame = reinterpret_cast<FRAME>(&token); // Opaque; fake backend never dereferences it.
        context.Font = reinterpret_cast<FONT>(&token);
        context.ForeGroundColor = Color{1, 2, 3, 255};
        context.BackGroundColor = Color{4, 5, 6, 255};
        context.Mode = DrawMode{DRAW_DEFAULT, 255};
        context.ClipRect = {{7, 8}, {9, 10}};
    }
    {
        auto handoff = uqm::suspendExplorationRendering(uqm::ExplorationView::Scan, &presentation);
        BOOST_REQUIRE(handoff);
        BOOST_TEST(callback == nullptr);
        for (auto *handle : handles) {
            auto &context = *handle;
            context.ForeGroundFrame = nullptr; context.Font = nullptr;
            context.ForeGroundColor.r = 99; context.BackGroundColor.g = 98;
            context.Mode.factor = 12; context.ClipRect = {};
        }
        active = PlanetContext; callback = childFrame; scale = 128; scaleMode = 3; origin = {11, 12};
    }
    BOOST_TEST(active == &contexts[3]);
    BOOST_TEST(callback == parentFrame);
    BOOST_TEST(scale == 256); BOOST_TEST(scaleMode == 1);
    BOOST_TEST(origin.x == 3); BOOST_TEST(origin.y == 4);
    for (const auto *handle : handles) {
        const auto &context = *handle;
        BOOST_TEST(context.ForeGroundFrame != nullptr); BOOST_TEST(context.Font != nullptr);
        BOOST_TEST(context.ForeGroundColor.r == 1u); BOOST_TEST(context.BackGroundColor.g == 5u);
        BOOST_TEST(context.Mode.factor == 255); BOOST_TEST(context.ClipRect.corner.x == 7);
        BOOST_TEST(context.ClipRect.extent.width == 9);
    }
    SpaceContext = RadarContext = active = nullptr;
}
BOOST_AUTO_TEST_CASE(flight_story_transition_does_not_create_an_orbital_render_context)
{
    active = nullptr; callback = parentFrame;
    auto handoff = uqm::suspendExplorationRendering(uqm::ExplorationView::Flight);
    BOOST_TEST(!handoff);
    BOOST_TEST(callback == parentFrame);
}

BOOST_AUTO_TEST_CASE(orbital_contexts_restore_parent_projections_and_failed_lazy_creation)
{
    uqm::GameSession session{std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Pilot"}), uqm::SinglePlayerId};
    SOLARSYS_STATE system{};
    const auto previous = PlanetContext;
    {
        uqm::OrbitPresentation parent{session, system};
        const auto planet = parent.planetContext();
        const auto scan = parent.scanContext();
        BOOLEAN owner = TRUE;
        BOOST_TEST(PlanetContext == planet);
        BOOST_TEST(GetScanContext(&owner) == scan);
        BOOST_TEST(!owner);
        {
            uqm::OrbitPresentation child{session, system};
            BOOST_TEST(PlanetContext != planet);
            failContext = true;
            BOOST_CHECK_THROW(child.scanContext(), std::runtime_error);
            BOOST_TEST(child.scanContext() != scan); // Failed load can be retried.
        }
        BOOST_TEST(PlanetContext == planet);
        BOOST_TEST(GetScanContext(nullptr) == scan);
        failContext = true;
        BOOST_CHECK_THROW((uqm::OrbitPresentation{session, system}), std::runtime_error);
        BOOST_TEST(PlanetContext == planet);
    }
    BOOST_TEST(PlanetContext == previous);
    BOOLEAN firstOwned = FALSE, secondOwned = FALSE;
    const auto first = GetScanContext(&firstOwned), second = GetScanContext(&secondOwned);
    BOOST_TEST(firstOwned); BOOST_TEST(secondOwned);
    BOOST_TEST(first != second); // Reports outside orbit own independent contexts.
    DestroyContext(first); DestroyContext(second);
    BOOST_CHECK_THROW(GetScanContext(nullptr), std::logic_error);
}

std::string lastLoadedTestMusicResource();
BOOST_AUTO_TEST_CASE(orbital_music_loads_only_its_selected_theme_and_releases_the_visit_lease)
{
    uqm::GameSession session{std::make_shared<uqm::GameState>(uqm::UserProfile{uqm::SinglePlayerId, "Pilot"}), uqm::SinglePlayerId};
    SOLARSYS_STATE system{};
    BOOST_TEST(session.assets().size() == 0u);
    {
        uqm::OrbitPresentation presentation{session, system};
        presentation.playMusic(0);
        BOOST_TEST(lastLoadedTestMusicResource() == "music.orbit1");
        BOOST_TEST(session.assets().size() == 1u);
        presentation.playMusic(9);
        BOOST_TEST(lastLoadedTestMusicResource() == "music.orbit5");
        BOOST_TEST(session.assets().size() == 1u);
    }
    BOOST_TEST(session.assets().size() == 0u);
}
