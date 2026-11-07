//Copyright Paul Reiche, Fred Ford. 1992-2002

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <array>
#include <cassert>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "options.h"
#include "strintrn.h"
#include "libs/log.h"
#include "libs/memlib.h"

namespace {
constexpr size_t MaxStrings = 2048;
constexpr size_t LineCapacity = 1024;
using Stream = std::unique_ptr<uio_Stream, decltype(&res_CloseResFile)>;

// Store a C string with its encoded terminator counted, as the legacy API
// requires. Empty/absent datasets instead use an empty std::string.
std::string terminated(std::string value) {
    value.push_back('\0');
    return value;
}

// Finish accumulated text, stripping only trailing line endings. A header
// with no body stays absent; a blank body keeps its one-byte terminator.
void finishText(StringEntry &entry) {
    auto &text = entry.text.data;
    if (text.empty())
        return;
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
        text.pop_back();
    text.push_back('\0');
}

// Parse a borrowed header line. Name/token views refer to line and must be
// copied before its next read. Delimiters match the existing resource syntax.
std::pair<std::string_view, std::string_view> header(std::string_view line) {
    const auto begin = line.find_first_not_of("()", 1);
    if (begin == line.npos)
        return {};
    const auto end = line.find_first_of("()", begin);
    const auto name = line.substr(begin, end == line.npos ? end : end - begin);
    if (end == line.npos)
        return {name, {}};
    const auto token = line.find_first_not_of(" \t\r\n)", end + 1);
    if (token == line.npos)
        return {name, {}};
    const auto tokenEnd = line.find_first_of(" \t\r\n)", token);
    return {name, line.substr(token, tokenEnd == line.npos ? tokenEnd : tokenEnd - token)};
}

// Parse text directly into complete entries, never packed parallel buffers.
// fp is required and borrowed at its current position; length bounds the read.
// Conversation mode retains names/clips; timestamp may be NULL. Any invalid
// timestamp record disables all timestamps, preserving the existing fallback.
std::vector<StringEntry> readEntries(uio_Stream *fp, const size_t length,
        const bool conversation, const std::string &clipPrefix,
        uio_Stream *timestamp) {
    std::vector<StringEntry> entries;
    std::array<char, LineCapacity> line{};
    const auto origin = uio_ftell(fp);
    bool timestampsValid = timestamp != nullptr;
    while (uio_fgets(line.data(), line.size(), fp)) {
        if (line[0] == '#') {
            const auto [name, clip] = header(line.data());
            if (!name.empty()) {
                if (entries.size() == MaxStrings)
                    break;
                if (!entries.empty())
                    finishText(entries.back());
                entries.emplace_back();
                auto &entry = entries.back();
                if (conversation) {
                    entry.name.data = terminated(std::string(name));
                    if (!clip.empty())
                        entry.soundClip = terminated(clipPrefix + std::string(clip));
                    if (timestampsValid) {
                        std::array<char, LineCapacity> stamp{};
                        const auto read = uio_fgets(stamp.data(), stamp.size(), timestamp);
                        const auto match = read && stamp[0] == '#'
                                ? std::strstr(stamp.data(), entry.name.data.c_str()) : nullptr;
                        if (!match) {
                            log_add(log_Warning, "Invalid timestamp data for '%s'. Disabling timestamps",
                                    entry.name.data.c_str());
                            timestampsValid = false;
                        } else {
                            // Skip the matching name and its closing delimiter.
                            auto value = match + name.size();
                            if (*value)
                                ++value;
                            value += std::strspn(value, " \t\r\n");
                            if (*value)
                                entry.timeStamp = terminated(value);
                        }
                    }
                }
            }
        } else if (!entries.empty()) {
            entries.back().text.data += line.data();
        }
        const auto position = uio_ftell(fp);
        if (position >= origin && static_cast<size_t>(position - origin) >= length)
            break;
    }
    if (!entries.empty())
        finishText(entries.back());
    if (!timestampsValid)
        for (auto &entry : entries)
            entry.timeStamp.clear();
    return entries;
}
}

void
_GetConversationData(const char *path, RESOURCE_DATA *resdata) try {
    assert(path && resdata);
    resdata->ptr = nullptr;
    const std::string paths(path);
    const auto first = paths.find(':');
    const auto second = first == paths.npos ? paths.npos : paths.find(':', first + 1);
    const auto filename = paths.substr(0, first);
    const auto clipPrefix = first == paths.npos ? std::string{} :
            paths.substr(first + 1, second == paths.npos ? second : second - first - 1);
    const Stream fp(res_OpenResFile(contentDir, filename.c_str(), "rb"), res_CloseResFile);
    if (!fp) {
        log_add(log_Warning, "Warning: Can't open '%s'", filename.c_str());
        return;
    }
    const auto length = LengthResFile(fp.get());
    if (!length)
        return;
    const Stream timestamp(second == paths.npos ? nullptr :
            uio_fopen(contentDir, paths.c_str() + second + 1, "rb"), res_CloseResFile);
    auto entries = readEntries(fp.get(), length, true, clipPrefix, timestamp.get());
    if (!entries.empty())
        resdata->ptr = MakeStringTable(std::move(entries)).release();
} catch (...) {
    std::terminate();
}

void *
_GetStringData(uio_Stream *fp, DWORD length) try {
    assert(fp);
    auto entries = readEntries(fp, length, false, {}, nullptr);
    return entries.empty() ? nullptr : MakeStringTable(std::move(entries)).release();
} catch (...) {
    std::terminate();
}

void *
_GetBinaryTableData(uio_Stream *fp, DWORD length) try {
    assert(fp);
    // The legacy resource prefix is consumed by GetResourceData. Check bounds
    // before interpreting word counts/offsets or copying arbitrary byte strings.
    constexpr size_t WordBytes = sizeof(DWORD);
    if (length < 3 * WordBytes)
        return nullptr;
    const std::unique_ptr<void, decltype(&HFree)> data(GetResourceData(fp, length), HFree);
    if (!data)
        return nullptr;
    const auto bytes = static_cast<const unsigned char *>(data.get());
    const size_t byteCount = length - WordBytes;
    const auto word = [bytes](size_t index) {
        const auto p = bytes + index * WordBytes;
        return (static_cast<DWORD>(p[0]) << 24) | (static_cast<DWORD>(p[1]) << 16)
                | (static_cast<DWORD>(p[2]) << 8) | p[3];
    };
    const size_t count = word(0);
    if (count > std::numeric_limits<COUNT>::max() || count + 2 > byteCount / WordBytes)
        return nullptr;
    const size_t skip = word(1);
    if (skip > byteCount / WordBytes - count - 2)
        return nullptr;
    size_t offset = (count + 2 + skip) * WordBytes;
    std::vector<StringEntry> entries(count);
    for (size_t i = 0; i < count; ++i) {
        const size_t size = word(i + 2);
        if (size > byteCount - offset)
            return nullptr;
        entries[i].text.data.assign(reinterpret_cast<const char *>(bytes + offset), size);
        offset += size;
    }
    return MakeStringTable(std::move(entries)).release();
} catch (...) {
    std::terminate();
}
