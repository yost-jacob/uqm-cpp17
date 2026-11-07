/*
 * Copyright (C) 2003  Serge van den Boom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * Nota bene: later versions of the GNU General Public License do not apply
 * to this program.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iterator>
#include <unordered_map>

#include "memdebug.h"

extern "C" {
void uio_DirHandle_print(FILE *out, const void *);
void uio_printMountInfo(FILE *out, const void *);
void uio_printMountTreeItem(FILE *out, const void *);
void uio_printPathComp(FILE *out, const void *);
}

const uio_MemDebug_LogTypeInfo uio_MemDebug_logTypeInfo[] = {
	{ "uio_DirHandle",      uio_DirHandle_print,    0 },
	{ "uio_FileSystemInfo", NULL,                   0 },
	{ "uio_GPDir",          NULL,                   0 },
	{ "uio_GPFile",         NULL,                   0 },
	{ "uio_GPRoot",         NULL,                   0 },
	{ "uio_Handle",         NULL,                   0 },
	{ "uio_MountHandle",    NULL,                   0 },
	{ "uio_MountInfo",      uio_printMountInfo,     0 },
	{ "uio_MountTree",      NULL,                   0 },
	{ "uio_MountTreeItem",  uio_printMountTreeItem, 0 },
	{ "uio_PathComp",       uio_printPathComp,      0 },
	{ "uio_PFileHandle",    NULL,                   0 },
	{ "uio_PDirHandle",     NULL,                   0 },
	{ "uio_PRoot",          NULL,                   0 },
	{ "uio_Repository",     NULL,                   0 },
	{ "uio_Stream",         NULL,                   0 },
	{ "stdio_GPDirData",    NULL,                   0 },
	{ "zip_GPFileData",     NULL,                   0 },
	{ "zip_GPDirData",      NULL,                   0 },
};

namespace {
// One value-owned reference count per borrowed address and resource type.
std::array<std::unordered_map<void *, int>, uio_MemDebug_numLogTypes> logs;
static_assert(std::size(uio_MemDebug_logTypeInfo) == std::size(logs));

/* Return the requested resource log. type must identify a real resource. */
auto &logFor(const uio_MemDebug_LogType type) {
    assert(type >= 0 && type < uio_MemDebug_numLogTypes);
    return logs[static_cast<size_t>(type)];
}

/* Require a non-NULL, registered address. References point into the log and
 * remain valid until this address is removed or the logs are cleared. */
int &referenceCount(const uio_MemDebug_LogType type, void *ptr) {
    auto &log = logFor(type);
    const auto found = log.find(ptr);
    if (!ptr || found == log.end()) {
        std::fprintf(stderr, "Fatal: Unregistered (%s *) %p.\n",
                uio_MemDebug_logTypeInfo[type].name, ptr);
        std::abort();
    }
    return found->second;
}

/* Print an action only when its flag is enabled. out/ptr are borrowed. */
void printAction(const uio_MemDebug_LogType type, void *ptr,
        const int flag, const char *action) {
    if (uio_MemDebug_logTypeInfo[type].flags & flag) {
        std::fprintf(stderr, "%s ", action);
        uio_MemDebug_printPointer(stderr, type, ptr);
        std::fputc('\n', stderr);
    }
}
} // namespace

void
uio_MemDebug_init(void) {
    for (auto &log : logs)
        log.clear();
}

void
uio_MemDebug_unInit(void) {
    for (auto &log : logs)
        log.clear();
}

void
uio_MemDebug_logAllocation(const uio_MemDebug_LogType type, void *ptr) try {
    auto &log = logFor(type);
    if (!ptr) {
        std::fprintf(stderr, "Fatal: Allocated (%s *) NULL.\n",
                uio_MemDebug_logTypeInfo[type].name);
        std::abort();
    }
    log.try_emplace(ptr, 1);
    printAction(type, ptr, uio_MemDebug_PRINT_ALLOC, "Alloc");
} catch (...) {
    // Allocation failure must never propagate an exception into C callers.
    std::terminate();
}

void
uio_MemDebug_logDeallocation(const uio_MemDebug_LogType type, void *ptr) {
    (void) referenceCount(type, ptr);
    printAction(type, ptr, uio_MemDebug_PRINT_FREE, "Free");
    logFor(type).erase(ptr);
}

void
uio_MemDebug_logRef(const uio_MemDebug_LogType type, void *ptr) {
    ++referenceCount(type, ptr);
    printAction(type, ptr, uio_MemDebug_PRINT_REF, "Ref++");
}

void
uio_MemDebug_logUnref(const uio_MemDebug_LogType type, void *ptr) {
    auto &count = referenceCount(type, ptr);
    if (count == 0) {
        std::fprintf(stderr, "Fatal: Reference count below zero for (%s *) %p.\n",
                uio_MemDebug_logTypeInfo[type].name, ptr);
        std::abort();
    }
    --count;
    printAction(type, ptr, uio_MemDebug_PRINT_UNREF, "Ref--");
}

void
uio_MemDebug_printPointer(FILE *out, const uio_MemDebug_LogType type, void *ptr) {
    assert(out);
    (void) logFor(type);
    const auto &info = uio_MemDebug_logTypeInfo[type];
    std::fprintf(out, "(%s *) %p", info.name, ptr);
    if (info.printFunction) {
        std::fprintf(out, ": ");
        info.printFunction(out, ptr);
    }
}

void
uio_MemDebug_printPointersType(FILE *out, const uio_MemDebug_LogType type) {
    assert(out);
    for (const auto &entry : logFor(type)) {
        uio_MemDebug_printPointer(out, type, entry.first);
        std::fputc('\n', out);
    }
}

void
uio_MemDebug_printPointers(FILE *out) {
    for (size_t i = 0; i < logs.size(); ++i)
        uio_MemDebug_printPointersType(out, static_cast<uio_MemDebug_LogType>(i));
}
