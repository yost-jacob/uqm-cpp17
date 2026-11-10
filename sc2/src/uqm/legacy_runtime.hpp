#ifndef UQM_LEGACY_RUNTIME_HPP_
#define UQM_LEGACY_RUNTIME_HPP_
#include "globdata.h"
#include <memory>
namespace uqm { class GameMode; class ArcadeSession; class AssetCache; }
namespace uqm::detail {
// Internal C buffer projection for a standalone runtime. These buffers remain
// owned by the session and must outlive the binding. Never attach during a
// live campaign. Native campaign callers continue to use their typed state.
struct LegacyRuntimeView { GLOBDATA &data; QUEUE &fleet; SIS_STATE &sis; AssetCache &assets; PLAYER_TRAVEL_POSITION travel{}; };
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
// Resource-loading C boundary only; native code borrows its explicit session cache.
AssetCache &sessionAssets();
// Construct the fleet-selection mode for this session. Its scoped frontend
// owns UI resources; execution starts only when the dispatcher selects it.
std::unique_ptr<GameMode> createArcadeFleetSelectionMode(ArcadeSession &session);
}
#endif
