#include "world_state.hpp"
#include "game_state.hpp"
#include "state.h"
#include "libs/log.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdio>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <utility>

// The C handle is opaque outside this file. Vector growth never moves the file
// record itself. All its members are guarded by the owning world's mutex.
struct GAME_STATE_FILE {
    const char *name;
    std::shared_mutex &mutex;
    std::vector<std::uint8_t> data;
    std::size_t cursor = 0;
    unsigned openCount = 0;
};
namespace uqm {
namespace detail {
struct WorldStorage {
    mutable std::shared_mutex mutex;
    std::array<GAME_STATE_FILE, 3> files{{
        {"STARINFO", mutex, {}}, {"RANDGRPINFO", mutex, {}}, {"DEFGRPINFO", mutex, {}}
    }};
};
struct LegacyWorldAccess {
    static GAME_STATE_FILE &file(WorldState &world, int index) {
        return world.storage_->files.at(static_cast<std::size_t>(index));
    }
};
}
namespace {
constexpr auto maxFileSize = std::numeric_limits<std::uint32_t>::max();
static_assert(STARINFO_FILE == 0 && RANDGRPINFO_FILE == 1 && DEFGRPINFO_FILE == 2);
}
WorldState::WorldState(UniverseId id) : id_(id), storage_(std::make_unique<detail::WorldStorage>()) {}
WorldState::~WorldState() = default;
UniverseId WorldState::id() const noexcept { return id_; }
WorldSnapshot WorldState::snapshot() const {
    const std::shared_lock lock(storage_->mutex);
    return {id_, storage_->files[0].data, storage_->files[1].data, storage_->files[2].data};
}
void WorldState::restore(WorldSnapshot candidate) {
    if (candidate.universe != id_)
        throw std::invalid_argument("world snapshot belongs to another universe");
    if (candidate.planetScans.size() > maxFileSize || candidate.randomGroups.size() > maxFileSize
            || candidate.scriptedGroups.size() > maxFileSize)
        throw std::length_error("world snapshot exceeds legacy file size limit");
    const std::unique_lock lock(storage_->mutex);
    for (const auto &file : storage_->files)
        if (file.openCount != 0)
            throw std::logic_error("cannot restore a world with open state files");
    storage_->files[0].data.swap(candidate.planetScans);
    storage_->files[1].data.swap(candidate.randomGroups);
    storage_->files[2].data.swap(candidate.scriptedGroups);
    for (auto &file : storage_->files)
        file.cursor = 0;
}
} // namespace uqm

// C adapters translate exceptions to existing failure results. Log diagnostics
// only after releasing world locks. Handles belong to the active world's owner.
extern "C" {
GAME_STATE_FILE *OpenStateFile(int stateFile, const char *mode) {
    if (stateFile < STARINFO_FILE || stateFile > DEFGRPINFO_FILE || !mode
            || (mode[0] != 'r' && mode[0] != 'w'))
        return nullptr;
    try {
        if (stateFile == STARINFO_FILE && mode[0] == 'w')
            ResetPersonalDiscoveries(); // New campaign or legacy checkpoint load.
        auto &file = uqm::detail::LegacyWorldAccess::file(uqm::gameSession().world(), stateFile);
        unsigned opens;
        {
            const std::unique_lock lock(file.mutex);
            if (file.openCount == std::numeric_limits<unsigned>::max())
                return nullptr;
            opens = ++file.openCount;
            if (mode[0] == 'w')
                file.data.clear();
            file.cursor = 0;
        }
        if (opens > 1)
            log_add(log_Warning, "State file %s open count is %u after open()", file.name, opens);
        return &file;
    } catch (...) { return nullptr; }
}
void CloseStateFile(GAME_STATE_FILE *file) {
    if (!file) return;
    try {
        bool wasOpen;
        {
            const std::unique_lock lock(file->mutex);
            wasOpen = file->openCount != 0;
            if (wasOpen) --file->openCount;
            file->cursor = 0;
        }
        if (!wasOpen)
            log_add(log_Warning, "State file %s closed without an open", file->name);
    } catch (...) {}
}
void DeleteStateFile(int stateFile) {
    if (stateFile < STARINFO_FILE || stateFile > DEFGRPINFO_FILE) return;
    try {
        auto &file = uqm::detail::LegacyWorldAccess::file(uqm::gameSession().world(), stateFile);
        unsigned opens;
        {
            const std::unique_lock lock(file.mutex);
            opens = file.openCount;
            std::vector<std::uint8_t>().swap(file.data);
            file.cursor = 0;
        }
        if (stateFile == STARINFO_FILE)
            ResetPersonalDiscoveries();
        if (opens != 0)
            log_add(log_Warning, "State file %s open count is %u during delete()", file.name, opens);
    } catch (...) {}
}
DWORD LengthStateFile(GAME_STATE_FILE *file) {
    if (!file) return 0;
    try {
        const std::shared_lock lock(file->mutex);
        return static_cast<DWORD>(file->data.size());
    } catch (...) { return 0; }
}
int ReadStateFile(void *buffer, COUNT size, COUNT count, GAME_STATE_FILE *file) {
    if (!file || !buffer || size == 0 || count == 0) return 0;
    try {
        const std::unique_lock lock(file->mutex);
        if (file->cursor >= file->data.size()) return 0;
        const auto available = (file->data.size() - file->cursor) / size;
        const auto elements = std::min<std::size_t>(count, available);
        const auto bytes = elements * size;
        if (bytes != 0) {
            std::memcpy(buffer, file->data.data() + file->cursor, bytes);
            file->cursor += bytes;
        }
        return static_cast<int>(elements);
    } catch (...) { return 0; }
}
int WriteStateFile(const void *buffer, COUNT size, COUNT count, GAME_STATE_FILE *file) {
    if (!file || !buffer || size == 0 || count == 0) return 0;
    try {
        const std::unique_lock lock(file->mutex);
        const auto bytes = static_cast<std::size_t>(size) * count;
        if (bytes > std::numeric_limits<DWORD>::max() - file->cursor) return 0;
        const auto end = file->cursor + bytes;
        if (end > file->data.size())
            file->data.resize(end); // Value initialization zero-fills a seek hole.
        std::memcpy(file->data.data() + file->cursor, buffer, bytes);
        file->cursor = end;
        return count;
    } catch (...) { return 0; }
}
int SeekStateFile(GAME_STATE_FILE *file, long offset, int whence) {
    if (!file) return 0;
    try {
        const std::unique_lock lock(file->mutex);
        std::int64_t base;
        switch (whence) {
            case SEEK_SET: base = 0; break;
            case SEEK_CUR: base = static_cast<std::int64_t>(file->cursor); break;
            case SEEK_END: base = static_cast<std::int64_t>(file->data.size()); break;
            default: return 0;
        }
        // Compare before addition so even LONG_MAX/MIN cannot overflow.
        if (offset < -base) {
            file->cursor = 0;
            return 0;
        }
        if (offset > static_cast<std::int64_t>(std::numeric_limits<DWORD>::max()) - base)
            return 0;
        file->cursor = static_cast<std::size_t>(base + offset);
        return 1;
    } catch (...) { return 0; }
}
} // extern "C"
