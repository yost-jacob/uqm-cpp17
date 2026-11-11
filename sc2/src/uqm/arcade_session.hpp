#ifndef UQM_ARCADE_SESSION_HPP_
#define UQM_ARCADE_SESSION_HPP_
#include "session.hpp"
#include <memory>
#include <utility>
namespace uqm {
// Standalone Super Melee visit. Owns its simulation/RNG and isolated temporary
// legacy buffers, with no campaign player or galaxy. The frontend owns its two
// setup sides. Enter only from the title boundary, after any campaign unwinds.
class ArcadeSession : public Session {
public:
    using Session::run;
    ArcadeSession(SessionDispatcher &host, std::uint32_t seed = std::random_device{}());
    SessionResult run() override;
    explicit ArcadeSession(std::uint32_t seed = std::random_device{}());
    ~ArcadeSession() override;
    ArcadeSession(const ArcadeSession &) = delete;
    ArcadeSession &operator=(const ArcadeSession &) = delete;
    SessionExit runArcade(); // Standalone/native mode execution; no application transition.
protected:
    detail::LegacyRuntimeView legacyRuntime() noexcept override;
private:
    struct Storage;
    std::unique_ptr<Storage> storage_;
};
}
#endif
