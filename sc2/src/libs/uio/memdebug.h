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

#ifndef LIBS_UIO_MEMDEBUG_H_
#define LIBS_UIO_MEMDEBUG_H_

#include <stdio.h>
#include "uioport.h"

#ifdef __cplusplus
extern "C" {
#endif

// Important: if you add an item here, add it to uio_MemDebug_LogTypeInfo
// too. The order should be the same.
typedef enum {
	uio_MemDebug_LogType_uio_DirHandle,
	uio_MemDebug_LogType_uio_FileSystemInfo,
	uio_MemDebug_LogType_uio_GPDir,
	uio_MemDebug_LogType_uio_GPFile,
	uio_MemDebug_LogType_uio_GPRoot,
	uio_MemDebug_LogType_uio_Handle,
	uio_MemDebug_LogType_uio_MountHandle,
	uio_MemDebug_LogType_uio_MountInfo,
	uio_MemDebug_LogType_uio_MountTree,
	uio_MemDebug_LogType_uio_MountTreeItem,
	uio_MemDebug_LogType_uio_PathComp,
	uio_MemDebug_LogType_uio_PFileHandle,
	uio_MemDebug_LogType_uio_PDirHandle,
	uio_MemDebug_LogType_uio_PRoot,
	uio_MemDebug_LogType_uio_Repository,
	uio_MemDebug_LogType_uio_Stream,
	uio_MemDebug_LogType_stdio_GPDirData,
	uio_MemDebug_LogType_zip_GPFileData,
	uio_MemDebug_LogType_zip_GPDirData,

	uio_MemDebug_numLogTypes,  /* This needs to be the last entry */
} uio_MemDebug_LogType;

typedef void (uio_MemDebug_PrintFunction)(FILE *out, const void *arg);

typedef struct uio_MemDebug_LogTypeInfo {
	const char *name;
	uio_MemDebug_PrintFunction *printFunction;
	int flags;
} uio_MemDebug_LogTypeInfo;

/* C-compatible constants: these flags never change at runtime. */
enum {
    uio_MemDebug_PRINT_ALLOC = 0x1,
    uio_MemDebug_PRINT_FREE = 0x2,
    uio_MemDebug_PRINT_REF = 0x4,
    uio_MemDebug_PRINT_UNREF = 0x8,
    uio_MemDebug_PRINT_ALL = uio_MemDebug_PRINT_ALLOC | uio_MemDebug_PRINT_FREE |
            uio_MemDebug_PRINT_REF | uio_MemDebug_PRINT_UNREF
};

extern const uio_MemDebug_LogTypeInfo uio_MemDebug_logTypeInfo[];

/* Callers must serialize access to these diagnostic logs. All addresses are
 * borrowed; logging never allocates or releases the resources being tracked. */

/* Start a fresh log; existing records are discarded without freeing resources. */
void uio_MemDebug_init(void);
/* Clear all records; tracked resources remain owned by their callers. */
void uio_MemDebug_unInit(void);
/* Register a borrowed, non-NULL address with count 1. type must be valid.
 * Duplicate registrations leave its count unchanged; allocation failure is fatal. */
void uio_MemDebug_logAllocation(uio_MemDebug_LogType type, void *ptr);
/* Remove a registered address without freeing it. Invalid addresses are fatal. */
void uio_MemDebug_logDeallocation(uio_MemDebug_LogType type, void *ptr);
/* Increment a registered address's diagnostic count. Invalid addresses are fatal. */
void uio_MemDebug_logRef(uio_MemDebug_LogType type, void *ptr);
/* Decrement a registered address's count; invalid addresses/underflow are fatal. */
void uio_MemDebug_logUnref(uio_MemDebug_LogType type, void *ptr);
/* Print a borrowed address using the given valid type; out must be non-NULL. */
void uio_MemDebug_printPointer(FILE *out, uio_MemDebug_LogType type,
		void *ptr);
/* Print all live addresses for a valid type to non-NULL out; order is unspecified. */
void uio_MemDebug_printPointersType(FILE *out, uio_MemDebug_LogType type);
/* Print every resource type to non-NULL out. No log is modified. */
void uio_MemDebug_printPointers(FILE *out);

#ifdef __cplusplus
}
#endif

#define uio_MemDebug_debugAlloc(type, pointer) \
	uio_MemDebug_logAllocation(uio_MemDebug_LogType_ ## type, pointer)
#define uio_MemDebug_debugFree(type, pointer) \
	uio_MemDebug_logDeallocation(uio_MemDebug_LogType_ ## type, pointer)
#define uio_MemDebug_debugRef(type, pointer) \
	uio_MemDebug_logRef(uio_MemDebug_LogType_ ## type, pointer)
#define uio_MemDebug_debugUnref(type, pointer) \
	uio_MemDebug_logUnref(uio_MemDebug_LogType_ ## type, pointer)

#endif  /* LIBS_UIO_MEMDEBUG_H_ */

