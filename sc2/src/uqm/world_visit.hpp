#ifndef UQM_WORLD_VISIT_HPP_
#define UQM_WORLD_VISIT_HPP_
#include <memory>
struct solarsys_state;
namespace uqm {
class GameSession;
class World;
// Synchronous visit controller. Session, system and selected world outlive this
// object and all its children. Owns the orbit location/graphics service scopes.
class WorldVisit {
public:
    WorldVisit(GameSession &session, solarsys_state &system, World &world);
    ~WorldVisit();
    WorldVisit(const WorldVisit &) = delete;
    WorldVisit &operator=(const WorldVisit &) = delete;
    // Invoke the constructor-selected script, or ordinary orbit for nullopt.
    // False means an exit/story handoff prevents resuming system flight.
    bool run();
    // Ordinary generation followed by an orbital child mode, unless exiting.
    void enterOrbit();
    // Existing system scripts retain their procedural story/encounter logic.
    // This adapter also presents any orbit surface prepared by that script.
    static void runSystemEvent(WorldVisit &visit);
private:
    bool canContinue() const;
    void presentOrbit();
    class Resources;
    GameSession &session_;
    solarsys_state &system_;
    World &world_;
    std::unique_ptr<Resources> resources_;
};
}
#endif
