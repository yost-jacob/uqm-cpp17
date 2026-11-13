#ifndef UQM_LEGACY_RUNTIME_HPP_
#define UQM_LEGACY_RUNTIME_HPP_
#include "globdata.h"
#include "session_activity.h"
#include <memory>
#include "star_map.hpp"
namespace uqm { class Session; class GameMode; class ArcadeSession; class AssetCache; class GameSession; class GameDefinitions; }
namespace uqm::detail {
// Internal, non-owning C projections selected on the gameplay thread. The
// session and every referenced buffer outlive this scoped binding. Frontend /
// arcade views have no campaign; native callers use their explicit references.
struct LegacyRuntimeView {
    GLOBDATA &data;
    QUEUE &fleet;
    SIS_STATE &sis;
    AssetCache &assets;
    PLAYER_TRAVEL_POSITION *travel; // Frontend/arcade only; campaign resolves its live variant.
    GameSession *campaign = nullptr;
    GameDefinitions *definitions = nullptr;
    std::optional<StarId> *currentStar = nullptr;
    UQM_SESSION_ACTIVITY *activities = nullptr;
    Session *owner = nullptr; // Only a scoped C adapter borrow, never ownership.
};
class LegacyRuntimeBinding {
public:
    explicit LegacyRuntimeBinding(LegacyRuntimeView &view) noexcept;
    ~LegacyRuntimeBinding();
    LegacyRuntimeBinding(const LegacyRuntimeBinding &) = delete;
    LegacyRuntimeBinding &operator=(const LegacyRuntimeBinding &) = delete;
private:
    LegacyRuntimeView *previous_;
};
LegacyRuntimeView *boundRuntime() noexcept;
LegacyRuntimeView *standaloneRuntime() noexcept;
// Resource-loading C boundary only; native code borrows its explicit session cache.
AssetCache &sessionAssets();
// Construct the fleet-selection mode for this session. Its scoped frontend
// owns UI resources; execution starts only when the dispatcher selects it.
std::unique_ptr<GameMode> createArcadeFleetSelectionMode(ArcadeSession &session);
}
#endif
