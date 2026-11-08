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

#include <cassert>
#include <deque>
#include <exception>
#include <memory>
#include <utility>

#define NETCONNECTION_INTERNAL
#include "netplay.h"
#include "netconnection.h"
#include "packetq.h"
#include "netsend.h"
#ifdef NETPLAY_DEBUG
#include "libs/log.h"
#endif

using OwnedPacket = std::unique_ptr<Packet, decltype(&Packet_delete)>;

struct PacketQueue {
    std::deque<OwnedPacket> packets;
};

PacketQueue *
PacketQueue_create(void) try {
    return new PacketQueue;
} catch (...) {
    std::terminate();
}

void
PacketQueue_delete(PacketQueue *queue) {
    delete queue;
}

void
queuePacket(NetConnection *conn, Packet *packet) try {
    assert(conn && conn->queue && packet && NetConnection_isConnected(conn));
    OwnedPacket owned(packet, Packet_delete);
    conn->queue->packets.push_back(std::move(owned));

#ifdef NETPLAY_DEBUG
	if (packetType(packet) != PACKET_BATTLEINPUT &&
			packetType(packet) != PACKET_CHECKSUM) {
		// Reporting BattleInput or Checksum would get so spammy that it
		// would slow down the battle.
		log_add(log_Debug, "NETPLAY: [%d] ==> Queueing packet of type %s.\n",
				NetConnection_getPlayerNr(conn),
				packetTypeData[packetType(packet)].name);
	}
#ifdef NETPLAY_DEBUG_FILE
	if (conn->debugFile != NULL) {
		uio_fprintf(conn->debugFile,
				"NETPLAY: [%d] ==> Queueing packet of type %s.\n",
				NetConnection_getPlayerNr(conn),
				packetTypeData[packetType(packet)].name);
	}
#endif  /* NETPLAY_DEBUG_FILE */
#endif  /* NETPLAY_DEBUG */
} catch (...) {
    std::terminate();
}

int
flushPacketQueue(NetConnection *conn) {
    assert(conn && conn->queue && NetConnection_isConnected(conn));
    auto &packets = conn->queue->packets;
    while (!packets.empty()) {
        if (sendPacket(conn, packets.front().get()) == -1)
            return -1;
        packets.pop_front();
    }
    return 0;
}
