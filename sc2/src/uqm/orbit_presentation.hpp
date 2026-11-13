#ifndef UQM_ORBIT_PRESENTATION_HPP_
#define UQM_ORBIT_PRESENTATION_HPP_
#include "game_assets.hpp"
#include <memory>
#include <type_traits>
struct solarsys_state;
namespace uqm {
class GameSession;
struct OrbitContextDeleter { void operator()(CONTEXT context) const noexcept; };
using OrbitContext = std::unique_ptr<std::remove_pointer_t<CONTEXT>, OrbitContextDeleter>;
// Visit-owned contexts and selected music. System graphics and session outlive
// this scope. Children borrow these handles; PlanetContext is only a C service
// projection, restored before the owned context is destroyed.
class OrbitPresentation {
public:
    OrbitPresentation(GameSession &session, solarsys_state &system);
    ~OrbitPresentation();
    OrbitPresentation(const OrbitPresentation &) = delete;
    OrbitPresentation &operator=(const OrbitPresentation &) = delete;
    CONTEXT planetContext() const noexcept { return planet_.get(); }
    CONTEXT scanContext(); // Lazy; one context per presentation, including children.
    void playMusic(BYTE planetType); // Shares the session asset; playback remains visit-local.
private:
    GameSession &session_;
    solarsys_state &system_;
    OrbitContext planet_, scan_;
    std::shared_ptr<const MusicAsset> music_;
    CONTEXT previousPlanet_;
    OrbitPresentation *previous_;
};
}
#endif
