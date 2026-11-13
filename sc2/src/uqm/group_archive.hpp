#ifndef UQM_GROUP_ARCHIVE_HPP_
#define UQM_GROUP_ARCHIVE_HPP_
#include "grpintrn.h"
namespace uqm {
class GameSession;
struct SystemGeometry;
// Session-owned encounter bookkeeping over its world's legacy group bytes.
// Calls run synchronously on the gameplay thread; no archive cursor survives a
// call. The session and initialized race/group/ship collections must be alive.
// Group IDs, header offsets and serialized layout retain their legacy meanings.
class GroupArchive {
public:
    explicit GroupArchive(GameSession &session) noexcept : session_(session) {}
    GroupArchive(const GroupArchive &) = delete;
    GroupArchive &operator=(const GroupArchive &) = delete;
    void reset(bool firstTime); // Clear random groups, and scripted groups for a new game.
    DWORD save(DWORD offset, BYTE group);
    bool load(DWORD offset, BYTE group); // Saved list or one fleet; not GROUP_INIT_IP.
    bool initialize(DWORD offset, const SystemGeometry &geometry, QUEUE &groups);
private:
    bool read(DWORD offset, BYTE group, const SystemGeometry *geometry, QUEUE &groups);
    void flush(GROUP_HEADER &, DWORD offset, BYTE group, GAME_STATE_FILE *file);
    GameSession &session_;
    BYTE lastEncountered_ = 0; // Serialized in group zero; never shared across sessions.
};
}
#endif
