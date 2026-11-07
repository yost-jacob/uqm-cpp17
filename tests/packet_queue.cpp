#include <boost/test/unit_test.hpp>

#include <cerrno>
#include <cstdlib>
#include <new>
#include <vector>

#define NETCONNECTION_INTERNAL
#include "uqm/supermelee/netplay/netconnection.h"
#include "uqm/supermelee/netplay/netsend.h"
#include "uqm/supermelee/netplay/packetq.h"

namespace {
std::vector<uint32> sent;
std::vector<uint32> destroyed;
int livePackets = 0;
int sendsBeforeFailure = -1;
int sendError = EAGAIN;

/* Identify a test ping without changing its production packet representation. */
uint32 packetId(const Packet &packet)
{
    return ntoh32(reinterpret_cast<const Packet_Ping &>(packet).id);
}

/* Allocate a packet with the same malloc/free ownership used by the C code. */
Packet *makePacket(const uint32 id)
{
    void *storage = std::malloc(sizeof(Packet_Ping));
    BOOST_REQUIRE(storage);
    auto *packet = new (storage) Packet_Ping{};
    packet->header.len = hton16(sizeof(Packet_Ping));
    packet->header.type = hton16(PACKET_PING);
    packet->id = hton32(id);
    ++livePackets;
    return reinterpret_cast<Packet *>(packet);
}

struct PacketQueueFixture {
    NetConnection connection{};

    PacketQueueFixture()
    {
        BOOST_REQUIRE(livePackets == 0);
        sent.clear();
        destroyed.clear();
        sendsBeforeFailure = -1;
        sendError = EAGAIN;
        connection.state = NetState_inSetup;
        connection.stateFlags.connected = true;
        connection.queue = PacketQueue_create();
    }

    ~PacketQueueFixture()
    {
        PacketQueue_delete(connection.queue);
        BOOST_TEST(livePackets == 0);
    }
};
}

// These transport/deallocation boundaries make blocking and disconnects
// deterministic. The queue and its ownership logic are production code.
extern "C" {
bool NetConnection_isConnected(const NetConnection *connection)
{
    return connection && connection->stateFlags.connected;
}

int sendPacket(NetConnection *, Packet *packet)
{
    if (sendsBeforeFailure == 0) {
        errno = sendError;
        return -1;
    }
    sent.push_back(packetId(*packet));
    if (sendsBeforeFailure > 0)
        --sendsBeforeFailure;
    return 0;
}

void Packet_delete(Packet *packet)
{
    destroyed.push_back(packetId(*packet));
    --livePackets;
    std::free(packet);
}
}

BOOST_FIXTURE_TEST_CASE(packet_queue_drains_in_fifo_order_and_can_be_reused, PacketQueueFixture)
{
    BOOST_TEST(flushPacketQueue(&connection) == 0);
    queuePacket(&connection, makePacket(1));
    queuePacket(&connection, makePacket(2));
    BOOST_TEST(livePackets == 2);
    BOOST_TEST(flushPacketQueue(&connection) == 0);
    queuePacket(&connection, makePacket(3));
    BOOST_TEST(flushPacketQueue(&connection) == 0);
    const std::vector<uint32> expected{1, 2, 3};
    BOOST_TEST(sent == expected, boost::test_tools::per_element());
    BOOST_TEST(destroyed == expected, boost::test_tools::per_element());
    BOOST_TEST(livePackets == 0);
}

BOOST_FIXTURE_TEST_CASE(packet_queue_retains_every_packet_when_initially_blocked, PacketQueueFixture)
{
    queuePacket(&connection, makePacket(1));
    queuePacket(&connection, makePacket(2));
    sendsBeforeFailure = 0;
    sendError = EWOULDBLOCK;
    const int result = flushPacketQueue(&connection);
    const int savedError = errno;
    BOOST_TEST(result == -1);
    BOOST_TEST(savedError == EWOULDBLOCK);
    BOOST_TEST(sent.empty());
    BOOST_TEST(destroyed.empty());
    queuePacket(&connection, makePacket(3));
    sendsBeforeFailure = -1;
    BOOST_TEST(flushPacketQueue(&connection) == 0);
    const std::vector<uint32> expected{1, 2, 3};
    BOOST_TEST(sent == expected, boost::test_tools::per_element());
    BOOST_TEST(destroyed == expected, boost::test_tools::per_element());
}

BOOST_FIXTURE_TEST_CASE(packet_queue_retries_only_the_unsent_suffix_after_partial_flush, PacketQueueFixture)
{
    for (uint32 id = 1; id <= 3; ++id)
        queuePacket(&connection, makePacket(id));
    sendsBeforeFailure = 1;
    const int result = flushPacketQueue(&connection);
    const int savedError = errno;
    BOOST_TEST(result == -1);
    BOOST_TEST(savedError == EAGAIN);
    const std::vector<uint32> first{1};
    BOOST_TEST(sent == first, boost::test_tools::per_element());
    BOOST_TEST(destroyed == first, boost::test_tools::per_element());
    BOOST_TEST(livePackets == 2);
    BOOST_TEST(flushPacketQueue(&connection) == -1);
    queuePacket(&connection, makePacket(4));
    sendsBeforeFailure = -1;
    BOOST_TEST(flushPacketQueue(&connection) == 0);
    const std::vector<uint32> expected{1, 2, 3, 4};
    BOOST_TEST(sent == expected, boost::test_tools::per_element());
    BOOST_TEST(destroyed == expected, boost::test_tools::per_element());
}

BOOST_FIXTURE_TEST_CASE(packet_queue_destruction_frees_packets_after_terminal_send_error, PacketQueueFixture)
{
    for (uint32 id = 1; id <= 3; ++id)
        queuePacket(&connection, makePacket(id));
    sendsBeforeFailure = 1;
    sendError = ECONNRESET;
    const int result = flushPacketQueue(&connection);
    const int savedError = errno;
    BOOST_TEST(result == -1);
    BOOST_TEST(savedError == ECONNRESET);
    BOOST_TEST(livePackets == 2);
    PacketQueue_delete(connection.queue);
    connection.queue = nullptr;
    const std::vector<uint32> expected{1, 2, 3};
    BOOST_TEST(destroyed == expected, boost::test_tools::per_element());
    const std::vector<uint32> delivered{1};
    BOOST_TEST(sent == delivered, boost::test_tools::per_element());
}
