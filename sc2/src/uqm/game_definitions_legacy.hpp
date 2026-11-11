#ifndef UQM_GAME_DEFINITIONS_LEGACY_HPP_
#define UQM_GAME_DEFINITIONS_LEGACY_HPP_
#include "game_definitions.hpp"
#include "starmap.h"
namespace uqm { class Session; }
namespace uqm::detail {
// Gameplay-thread C adapters only. Native callers borrow Session::definitions().
GameDefinitions &activeDefinitions();
std::string readDefinitionFile(const char *path);
struct DefinitionsAccess {
    static GameDefinitions &storage(Session &);
    static void loadStars(GameDefinitions &, std::istream &);
    static void loadShips(GameDefinitions &, std::istream &);
    static void clearStars(GameDefinitions &);
    static STAR_DESC *descriptor(GameDefinitions &, COUNT);
    static COUNT index(const GameDefinitions &, const STAR_DESC *);
};
}
#endif
