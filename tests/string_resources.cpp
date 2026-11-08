#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>

#include "libs/strings/strintrn.h"
#include "libs/memlib.h"
#include "libs/log.h"
#include "options.h"

// In-memory VFS boundary for the real loaders, allocator and table destructor.
// Tests control file bytes and count live handles without starting the game.
struct uio_Stream {
    std::string bytes;
    size_t position = 0;
};

namespace {
std::unordered_map<std::string, std::string> files;
int liveStreams = 0;
using Table = std::unique_ptr<STRING_TABLE_DESC, decltype(&FreeStringTable)>;
using Stream = std::unique_ptr<uio_Stream, decltype(&res_CloseResFile)>;

struct ResourceFixture {
    ResourceFixture() { files.clear(); BOOST_REQUIRE(liveStreams == 0); }
    ~ResourceFixture() { BOOST_TEST(liveStreams == 0); }
};

/* Load registered file bytes and own the returned C table, if any. */
Table conversation(const char *path)
{
    RESOURCE_DATA data{};
    _GetConversationData(path, &data);
    BOOST_TEST(liveStreams == 0);
    return Table(static_cast<STRING_TABLE_DESC *>(data.ptr), FreeStringTable);
}
}

extern "C" {
uio_DirHandle *contentDir = nullptr;

// The loader's diagnostic output is irrelevant to fixture ownership checks.
void log_add(log_Level, const char *, ...) {}

uio_Stream *uio_fopen(uio_DirHandle *, const char *path, const char *)
{
    const auto found = files.find(path);
    if (found == files.end())
        return nullptr;
    ++liveStreams;
    return new uio_Stream{found->second};
}

uio_Stream *res_OpenResFile(uio_DirHandle *dir, const char *path, const char *mode)
{
    return uio_fopen(dir, path, mode);
}

int uio_fclose(uio_Stream *stream)
{
    --liveStreams;
    delete stream;
    return 0;
}

BOOLEAN res_CloseResFile(uio_Stream *stream)
{
    if (stream)
        uio_fclose(stream);
    return TRUE;
}

size_t LengthResFile(uio_Stream *stream) { return stream->bytes.size(); }
long uio_ftell(uio_Stream *stream) { return static_cast<long>(stream->position); }

char *uio_fgets(char *buffer, int size, uio_Stream *stream)
{
    if (stream->position == stream->bytes.size())
        return nullptr;
    const auto newline = stream->bytes.find('\n', stream->position);
    const auto end = newline == std::string::npos ? stream->bytes.size() : newline + 1;
    const auto count = std::min(end - stream->position, static_cast<size_t>(size - 1));
    std::memcpy(buffer, stream->bytes.data() + stream->position, count);
    buffer[count] = '\0';
    stream->position += count;
    return buffer;
}

size_t ReadResFile(void *buffer, size_t size, size_t count, uio_Stream *stream)
{
    const auto available = (stream->bytes.size() - stream->position) / size;
    count = std::min(count, available);
    std::memcpy(buffer, stream->bytes.data() + stream->position, size * count);
    stream->position += size * count;
    return count;
}

BOOLEAN FreeResourceData(void *data) { HFree(data); return TRUE; }
}

BOOST_FIXTURE_TEST_CASE(conversation_owns_all_datasets_and_closes_both_files, ResourceFixture)
{
    files["dialog"] = "#(HELLO) hello.ogg\nHello\nthere\n\n#(BYE) bye.ogg\nBye\n";
    files["timestamps"] = "#(HELLO) 10,20\n#(BYE) 30\n";
    const auto table = conversation("dialog:voice/:timestamps");
    BOOST_REQUIRE(table);
    BOOST_TEST(table->size == 2);
    BOOST_TEST(std::string(table->strings[0].data) == "Hello\nthere");
    BOOST_TEST(std::string(GetStringName(&table->strings[0])) == "HELLO");
    BOOST_TEST(std::string(GetStringSoundClip(&table->strings[0])) == "voice/hello.ogg");
    BOOST_TEST(std::string(GetStringTimeStamp(&table->strings[1])) == "30\n");
    BOOST_TEST(GetStringByName(table.get(), "BYE") == &table->strings[3]);
}

BOOST_FIXTURE_TEST_CASE(conversation_releases_missing_empty_and_headerless_inputs, ResourceFixture)
{
    BOOST_TEST(!conversation("missing"));
    files["empty"] = "";
    files["headerless"] = "ignored preamble\n";
    BOOST_TEST(!conversation("empty"));
    BOOST_TEST(!conversation("headerless"));
    files["blank-entry"] = "#(EMPTY)\n";
    const auto table = conversation("blank-entry");
    BOOST_REQUIRE(table);
    BOOST_TEST(table->strings[0].length == 0);
    BOOST_TEST(table->strings[0].data == nullptr);
}

BOOST_FIXTURE_TEST_CASE(conversation_drops_incomplete_timestamps_and_keeps_text, ResourceFixture)
{
    files["dialog"] = "#(FIRST)\nFirst\n#(SECOND)\nSecond\n";
    files["timestamps"] = "#(FIRST) 10\n";
    const auto table = conversation("dialog::timestamps");
    BOOST_REQUIRE(table);
    BOOST_TEST((table->flags & HAS_TIMESTAMP) == 0);
    BOOST_TEST(std::string(table->strings[1].data) == "Second");
    BOOST_TEST(GetStringTimeStamp(&table->strings[0]) == nullptr);
}

BOOST_FIXTURE_TEST_CASE(text_loader_borrows_its_stream_and_grows_temporary_storage, ResourceFixture)
{
    const std::string longLine(900, 'x');
    std::string expected;
    files["text"] = "#(FIRST)\n";
    for (int i = 0; i < 12; ++i) {
        files["text"] += longLine + '\n';
        expected += longLine + '\n';
    }
    expected.pop_back();
    files["text"] += "#(SECOND)\nSecond\r\n\n";
    const Stream stream(res_OpenResFile(nullptr, "text", "rb"), res_CloseResFile);
    const Table table(static_cast<STRING_TABLE_DESC *>(_GetStringData(stream.get(), LengthResFile(stream.get()))), FreeStringTable);
    BOOST_TEST(liveStreams == 1);
    BOOST_REQUIRE(table);
    BOOST_TEST(std::string(table->strings[0].data) == expected);
    BOOST_TEST(std::string(table->strings[1].data) == "Second");
}

BOOST_FIXTURE_TEST_CASE(binary_loader_owns_temporary_bytes_and_preserves_embedded_nuls, ResourceFixture)
{
    std::string bytes;
    for (const unsigned int word : {0xffffffffu, 2u, 0u, 4u, 3u})
        for (int shift = 24; shift >= 0; shift -= 8)
            bytes += static_cast<char>((word >> shift) & 0xff);
    bytes.append("one\0x\0y", 7);
    files["binary"] = bytes;
    const Stream stream(res_OpenResFile(nullptr, "binary", "rb"), res_CloseResFile);
    const Table table(static_cast<STRING_TABLE_DESC *>(_GetBinaryTableData(stream.get(), bytes.size())), FreeStringTable);
    BOOST_REQUIRE(table);
    BOOST_TEST(table->size == 2);
    BOOST_TEST(std::string(table->strings[0].data, table->strings[0].length) == std::string("one\0", 4));
    BOOST_TEST(std::string(table->strings[1].data, table->strings[1].length) == std::string("x\0y", 3));
    BOOST_TEST(liveStreams == 1);
}
