#ifndef UQM_EXPLORATION_RENDERING_HPP_
#define UQM_EXPLORATION_RENDERING_HPP_
#include "game_mode.hpp"
namespace uqm {
enum class ExplorationView;
class OrbitPresentation;
// Capture only an established parent view. The RAII token borrows parent-owned
// contexts and restores callback/drawing bindings after the child is destroyed.
std::unique_ptr<ModeSuspension> suspendExplorationRendering(ExplorationView view, OrbitPresentation *presentation = nullptr);
}
#endif
