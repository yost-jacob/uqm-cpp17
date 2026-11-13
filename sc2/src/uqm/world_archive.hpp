#ifndef UQM_WORLD_ARCHIVE_HPP_
#define UQM_WORLD_ARCHIVE_HPP_
#include "state.h"
namespace uqm { class WorldState; }
namespace uqm::detail {
// Owner-explicit access to a legacy archive cursor. Same mode/serialization
// contract as OpenStateFile; caller closes the handle before its world dies.
// Does not reset personal discoveries (that is a C campaign reset operation).
GAME_STATE_FILE *openStateFile(WorldState &world, int file, const char *mode);
}
#endif
