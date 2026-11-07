/*
 *  Copyright 2006  Serge van den Boom <svdb@stack.nl>
 *
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef UQM_SUPERMELEE_NETPLAY_PACKETQ_H_
#define UQM_SUPERMELEE_NETPLAY_PACKETQ_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PacketQueue PacketQueue;
typedef struct NetConnection NetConnection;
typedef struct Packet Packet;

/* Create an empty owned FIFO queue. The connection must delete it exactly once.
 * Allocation failure terminates without propagating C++ exceptions into C. */
PacketQueue *PacketQueue_create(void);

/* Delete the queue and all packets it still owns. NULL is allowed. No operation
 * may access this queue during or after deletion. */
void PacketQueue_delete(PacketQueue *queue);

/* Append packet to conn's queue, transferring exclusive ownership immediately.
 * conn must be connected and own a live queue; packet must be non-NULL and
 * releasable with Packet_delete. The caller must not reuse/delete packet.
 * Callers must serialize queue operations; allocation failure is fatal. */
void queuePacket(NetConnection *conn, Packet *packet);

/* Send queued packets in FIFO order, deleting each only after sendPacket
 * succeeds. Return 0 when drained, or -1 with sendPacket's errno unchanged.
 * On any send error, retain the failed packet and all subsequent packets for
 * retry or destruction. conn must be connected and own a live queue.
 * Transport callbacks must not reenter or destroy this queue during sending. */
int flushPacketQueue(NetConnection *conn);

#ifdef __cplusplus
}
#endif
#endif /* UQM_SUPERMELEE_NETPLAY_PACKETQ_H_ */
