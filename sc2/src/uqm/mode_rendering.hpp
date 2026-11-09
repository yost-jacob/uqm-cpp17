#ifndef UQM_MODE_RENDERING_HPP_
#define UQM_MODE_RENDERING_HPP_
#include "game_mode.hpp"
#include "libs/gfxlib.h"
#include <initializer_list>
namespace uqm {
// Contexts/resources remain owned by the suspended parent. Snapshot destruction
// restores drawing bindings and the frame callback after child teardown.
std::unique_ptr<ModeSuspension> suspendDrawingBindings(std::initializer_list<CONTEXT> contexts);
}
#endif
