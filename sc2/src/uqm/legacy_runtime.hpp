#ifndef UQM_LEGACY_RUNTIME_HPP_
#define UQM_LEGACY_RUNTIME_HPP_
#include "globdata.h"
namespace uqm::detail {
// Internal C buffer projection for a standalone runtime. These buffers remain
// owned by the session and must outlive the binding. Never attach during a
// live campaign. Native campaign callers continue to use their typed state.
struct LegacyRuntimeView { GLOBDATA &data; QUEUE &fleet; SIS_STATE &sis; };
class LegacyRuntimeBinding {
public:
    explicit LegacyRuntimeBinding(LegacyRuntimeView &view) noexcept;
    ~LegacyRuntimeBinding();
    LegacyRuntimeBinding(const LegacyRuntimeBinding &) = delete;
    LegacyRuntimeBinding &operator=(const LegacyRuntimeBinding &) = delete;
private:
    LegacyRuntimeView *previous_;
};
LegacyRuntimeView *standaloneRuntime() noexcept;
// Runs the old setup/input frontend against the selected arcade session.
// Owns its UI resources through a scope; does not create or replace a campaign.
void runArcadeFrontend();
}
#endif
