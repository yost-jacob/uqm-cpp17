#include <boost/test/unit_test.hpp>

#include <cstdio>
#include <memory>

#include "libs/uio/memdebug.h"

// The bookkeeping tests do not link the VFS. These print hooks stand in for
// its diagnostic functions; resource tracking itself is the production code.
extern "C" {
void uio_DirHandle_print(FILE *, const void *) {}
void uio_printMountInfo(FILE *, const void *) {}
void uio_printMountTreeItem(FILE *, const void *) {}
void uio_printPathComp(FILE *, const void *) {}
}

BOOST_AUTO_TEST_CASE(memory_log_tracks_addresses_without_owning_them)
{
    constexpr auto type = uio_MemDebug_LogType_uio_Handle;
    int resource = 42;
    uio_MemDebug_init();
    uio_MemDebug_logAllocation(type, &resource);
    uio_MemDebug_logRef(type, &resource);
    uio_MemDebug_logUnref(type, &resource);
    uio_MemDebug_logUnref(type, &resource);
    const std::unique_ptr<FILE, int (*)(FILE *)> output(std::tmpfile(), std::fclose);
    BOOST_REQUIRE(output);
    uio_MemDebug_printPointersType(output.get(), type);
    BOOST_TEST(std::ftell(output.get()) > 0);
    const auto position = std::ftell(output.get());
    uio_MemDebug_logDeallocation(type, &resource);
    uio_MemDebug_printPointersType(output.get(), type);
    BOOST_TEST(std::ftell(output.get()) == position);
    uio_MemDebug_unInit();
    BOOST_TEST(resource == 42);
}

BOOST_AUTO_TEST_CASE(memory_log_clear_releases_records_for_all_types)
{
    constexpr auto type = uio_MemDebug_LogType_uio_Handle;
    int resource = 42;
    uio_MemDebug_init();
    uio_MemDebug_logAllocation(type, &resource);
    uio_MemDebug_unInit();
    const std::unique_ptr<FILE, int (*)(FILE *)> output(std::tmpfile(), std::fclose);
    BOOST_REQUIRE(output);
    uio_MemDebug_printPointers(output.get());
    BOOST_TEST(std::ftell(output.get()) == 0);
}
