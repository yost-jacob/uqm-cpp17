//Copyright Paul Reiche, Fred Ford. 1992-2002
// SPDX-License-Identifier: GPL-2.0-or-later
#include "simulation_engine.hpp"
#include "libs/log.h"
#include "libs/misc.h"
#include <stdexcept>
namespace uqm {
std::optional<std::reference_wrapper<ELEMENT>> SimulationEngine::allocateElement() {
    if (!initialized()) throw std::logic_error("element allocation requires an initialized engine");
    SimulationBinding binding{*this};
    const auto handle = AllocLink(&entities_);
    if (!handle) return std::nullopt;
    auto &element = *static_cast<ELEMENT *>(handle);
    element = {};
    element.PrimIndex = AllocDisplayPrim();
    if (element.PrimIndex == END_OF_LIST) {
        // Preserve the existing fatal policy for this internal bookkeeping error.
        // Ordinary entity-pool exhaustion remains a normal optional failure.
        log_add(log_Error, "AllocElement: Out of display prims!");
        explode();
    }
    SetPrimType(&DisplayArray[element.PrimIndex], NO_PRIM);
    return std::ref(element);
}
void SimulationEngine::enqueue(ELEMENT &element) noexcept { PutQueue(&entities_, &element); }
void SimulationEngine::releaseElement(ELEMENT &element) noexcept {
    SimulationBinding binding{*this};
    FreeDisplayPrim(element.PrimIndex);
    FreeLink(&entities_, &element);
}
}
// C callbacks borrow the selected owner only here. Native callers use optional
// references and explicit engine lifetime contracts.
HELEMENT AllocElement(void) try {
    const auto element = uqm::simulationEngine().allocateElement();
    return element ? &element->get() : nullptr;
} catch (...) {
    log_add(log_Error, "AllocElement: invalid engine allocation request.");
    explode();
}
void FreeElement(HELEMENT handle) {
    if (handle) uqm::simulationEngine().releaseElement(*static_cast<ELEMENT *>(handle));
}
