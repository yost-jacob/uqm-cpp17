#ifndef UQM_ENTITY_HANDLE_HPP_
#define UQM_ENTITY_HANDLE_HPP_
#include "element.h"
#include <cstdint>
#include <functional>
#include <optional>
namespace uqm::combat {
// Non-owning identity of one ELEMENT allocation. Reusing the same pool address
// cannot make an old handle refer to a replacement object. No object is kept
// alive, and resolve never returns a reference after pool free/reset/teardown.
class EntityHandle {
public:
    EntityHandle() = default;
    // element must be an allocation in queue; unowned engine ships are valid too.
    static EntityHandle capture(const QUEUE &queue, const ELEMENT &element) noexcept;
    // The supplied queue must itself be alive. The returned borrow lasts until
    // that allocation is freed; callers must not retain it across engine ticks.
    std::optional<std::reference_wrapper<ELEMENT>> resolve(const QUEUE &queue) const noexcept;
private:
    ELEMENT *slot_ = nullptr;
    std::uint64_t generation_ = 0;
};
}
#endif
