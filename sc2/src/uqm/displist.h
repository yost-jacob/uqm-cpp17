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

#ifndef UQM_DISPLIST_H_
#define UQM_DISPLIST_H_

#include <string.h>
#include "libs/compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *HLINK;
/* Every supported C game object begins with these two handle fields. */
typedef struct link { HLINK pred, succ; } LINK;

typedef enum {
    QUEUE_ELEMENT, QUEUE_STARSHIP, QUEUE_SHIP_FRAGMENT, QUEUE_FLEET_INFO,
    QUEUE_IP_GROUP, QUEUE_MASTER_SHIP
} QueueKind;

typedef struct QueueStorage QueueStorage;
/* Owns a fixed-capacity typed pool. Zero-initialize before first InitQueue;
 * never copy an owning QUEUE. Use SwapQueues to transfer ownership. Callers
 * serialize access and release external payload resources before UninitQueue. */
typedef struct {
    HLINK head, tail;
    QueueStorage *storage;
    COUNT object_size;
} QUEUE;

/* Initialize an empty/zeroed queue with exactly capacity objects of kind.
 * Capacity never grows: notably the encounter limit controls gameplay.
 * Allocation failure terminates; zero capacity is valid. Returns TRUE. */
BOOLEAN InitQueue(QUEUE *queue, COUNT capacity, QueueKind kind);
/* Destroy the typed pool, invalidating all handles/pointers. Does not release
 * external resources referenced by game objects. Empty queues are allowed. */
BOOLEAN UninitQueue(QUEUE *queue);
/* Return every slot to the pool in original order and empty the linked queue.
 * Payload bytes stay intact; all previous reservations/links become invalid. */
void ReinitQueue(QUEUE *queue);
/* Swap complete ownership and order, preserving handles to both pools. Both
 * arguments must point to initialized or zero-initialized queue descriptors. */
void SwapQueues(QUEUE *left, QUEUE *right);
/* Reserve an unlinked slot, returning NULL on exhaustion. Storage addresses
 * stay stable until UninitQueue. Initial allocation order is ascending, and
 * released slots are reused last-in first-out. Payload is not cleared. */
HLINK AllocLink(QUEUE *queue);
/* Return a reserved, unlinked, non-NULL slot to its owning pool. RemoveQueue
 * first if linked. Never double-free or use a handle after releasing it. */
void FreeLink(QUEUE *queue, HLINK link);
/* Borrow a slot address; NULL returns NULL. A non-NULL handle must refer to an
 * object boundary in this pool. Unlock validates the same contract; no pinning
 * or synchronization is needed because published vectors never resize. */
LINK *LockLink(const QUEUE *queue, HLINK link);
void UnlockLink(const QUEUE *queue, HLINK link);
/* Append a reserved unlinked slot, or insert it before ref (NULL appends).
 * Both handles must belong to this queue, and ref must be linked when given. */
void PutQueue(QUEUE *queue, HLINK link);
void InsertQueue(QUEUE *queue, HLINK link, HLINK ref);
/* Unlink a linked slot without releasing it. Saved successor/predecessor
 * handles remain usable; callers may reinsert or FreeLink the removed slot. */
void RemoveQueue(QUEUE *queue, HLINK link);
COUNT CountLinks(const QUEUE *queue);
/* Visit in link order, saving the successor before callback. callback must be
 * non-NULL and may remove/free its current slot, but must keep its saved next
 * slot alive. arg is borrowed and may be NULL. Callbacks must not throw. */
void ForAllLinks(QUEUE *queue, void (*callback)(LINK *, void *), void *arg);

/* Read-only queue queries; the queue argument must be non-NULL. */
static inline COUNT GetLinkSize(const QUEUE *queue) { return queue->object_size; }
static inline HLINK GetHeadLink(const QUEUE *queue) { return queue->head; }
static inline HLINK GetTailLink(const QUEUE *queue) { return queue->tail; }
/* Read the common prefix of any supported object without aliasing its concrete
 * type as LINK. object must point to a valid game object, never NULL. */
static inline HLINK _GetPredLink(const void *object) {
    HLINK result;
    memcpy(&result, object, sizeof result);
    return result;
}
static inline HLINK _GetSuccLink(const void *object) {
    HLINK result;
    memcpy(&result, (const unsigned char *)object + sizeof(HLINK), sizeof result);
    return result;
}

#ifdef __cplusplus
}
#endif
#endif /* UQM_DISPLIST_H_ */
