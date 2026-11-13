#include "orbit_presentation.hpp"
#include "game_state.hpp"
#include "planet_sphere.hpp"
#include "planets/scan.h"
#include "setup.h"
#include "settings.h"
#include <stdexcept>
#include <utility>

// Compatibility projection for lander/sphere drawing helpers without a visit
// parameter. It never owns a context. Native orbit/scan code uses its visit.
CONTEXT PlanetContext = nullptr;
namespace {
thread_local uqm::OrbitPresentation *presentation = nullptr;
struct ContextRestore {
    CONTEXT previous = GetContext();
    ~ContextRestore() { SetContext(previous); }
};
uqm::OrbitContext createContext(bool scan) {
    ContextRestore restore;
    SetContext(SpaceContext);
    RECT bounds{};
    GetContextClipRect(&bounds);
    uqm::OrbitContext context{CreateContext(scan ? "ScanContext" : "PlanetContext")};
    if (!context) throw std::runtime_error("could not create orbital drawing context");
    SetContext(context.get());
    SetContextFGFrame(Screen);
    if (scan) {
        bounds.corner.x += bounds.extent.width - MAP_WIDTH;
        bounds.corner.y += bounds.extent.height - MAP_HEIGHT;
        bounds.extent = {static_cast<COORD>(MAP_WIDTH), static_cast<COORD>(MAP_HEIGHT)};
    } else bounds.extent.height -= MAP_HEIGHT + MAP_BORDER_HEIGHT;
    SetContextClipRect(&bounds);
    return context;
}
}
namespace uqm {
void OrbitContextDeleter::operator()(CONTEXT context) const noexcept { if (context) DestroyContext(context); }
OrbitPresentation::OrbitPresentation(GameSession &session, SOLARSYS_STATE &system)
    : session_(session), system_(system), planet_(createContext(false)),
      previousPlanet_(std::exchange(PlanetContext, planet_.get())),
      previous_(std::exchange(presentation, this)) {}
OrbitPresentation::~OrbitPresentation() {
    if (system_.graphics) UninitSphereRotation(system_.graphics->Orbit);
    StopMusic();
    // Stop borrowing these contexts before releasing them. Parent drawing
    // bindings are restored by the dispatcher's handback token afterward.
    SetContext(SpaceContext);
    presentation = previous_;
    PlanetContext = previousPlanet_;
}
CONTEXT OrbitPresentation::scanContext() {
    if (!scan_) scan_ = createContext(true);
    return scan_.get();
}
void OrbitPresentation::playMusic(BYTE planetType) {
    constexpr unsigned ThemeCount = 5;
    const auto id = "music.orbit" + std::to_string(planetType % ThemeCount + 1);
    if (music_) StopMusic();
    music_ = std::static_pointer_cast<const MusicAsset>(session_.assets().find({AssetKind::Music, id}));
    if (!PLRPlaying(reinterpret_cast<MUSIC_REF>(~std::uintptr_t{0})))
        PlayMusic(music_->music(), TRUE, 1);
}
}
// C reports may precede surface presentation. Return an independently owned
// context in that case; the report destroys that handle directly. No singleton
// owns a spare scan context, and reports inside orbit borrow the visit's owner.
CONTEXT GetScanContext(BOOLEAN *owner) {
    if (presentation) {
        if (owner) *owner = FALSE;
        return presentation->scanContext();
    }
    if (!owner) throw std::logic_error("scan context requires an orbital presentation");
    *owner = TRUE;
    return createContext(true).release();
}
