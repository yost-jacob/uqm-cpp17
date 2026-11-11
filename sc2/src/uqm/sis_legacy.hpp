#ifndef UQM_SIS_LEGACY_HPP_
#define UQM_SIS_LEGACY_HPP_
#include "sis_state.hpp"
namespace uqm::detail {
// C buffer boundary only. The record remains owned by Fleet's optional SIS.
struct SISAccess {
    static SIS_STATE &record(SIS &sis) noexcept { return sis.data_; }
    static const SIS_STATE &record(const SIS &sis) noexcept { return sis.data_; }
};
}
#endif
