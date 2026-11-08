#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_map>

#include "libs/strings/strintrn.h"
#include "libs/memlib.h"
#include "libs/log.h"
#include "options.h"
#include "uqm/starmap.h"

// In-memory VFS boundary for the real loaders, allocator and table destructor.
// Tests control file bytes and count live handles without starting the game.
struct uio_Stream {
    std::string bytes;
    size_t position = 0;
};

namespace {
std::unordered_map<std::string, std::string> files;
int liveStreams = 0;
std::unordered_map<std::string, ResourceFreeFun *> registeredDeleters;
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
const char *_cur_resfile_name = nullptr;

BOOLEAN InstallResTypeVectors(const char *type, ResourceLoadFun *,
        ResourceFreeFun *freeFun, ResourceStringFun *) {
    registeredDeleters[type] = freeFun;
    return TRUE;
}

// Resource installation links these instance-lookup entry points; fixtures
// load file bytes directly and never create a global resource index.
void *res_GetResource(RESOURCE) { return nullptr; }
void *res_DetachResource(RESOURCE) { return nullptr; }
void *LoadResourceFromPath(const char *path, ResourceLoadFileFun *load) {
    const Stream stream(res_OpenResFile(contentDir, path, "rb"), res_CloseResFile);
    return stream ? load(stream.get(), LengthResFile(stream.get())) : nullptr;
}

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
    const auto first = CaptureStringTable(table.get());
    BOOST_TEST(GetStringTableCount(first) == 2u);
    BOOST_TEST(std::string(GetStringAddress(first)) == "Hello\nthere");
    BOOST_TEST(std::string(GetStringName(first)) == "HELLO");
    BOOST_TEST(std::string(GetStringSoundClip(first)) == "voice/hello.ogg");
    BOOST_TEST(std::string(GetStringTimeStamp(SetAbsStringTableIndex(first, 1))) == "30\n");
    const auto named = GetStringByName(table.get(), "BYE");
    BOOST_REQUIRE(named);
    BOOST_TEST(GetStringTableIndex(named) == 3u);
    BOOST_TEST(std::string(GetStringAddress(named)) == "BYE");
    BOOST_TEST(SetRelStringTableIndex(named, 0) ==
            SetAbsStringTableIndex(first, 1));
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
    const auto first = CaptureStringTable(table.get());
    BOOST_TEST(GetStringLengthBin(first) == 0u);
    BOOST_TEST(GetStringAddress(first) == nullptr);
}

BOOST_FIXTURE_TEST_CASE(conversation_drops_incomplete_timestamps_and_keeps_text, ResourceFixture)
{
    files["dialog"] = "#(FIRST)\nFirst\n#(SECOND)\nSecond\n";
    files["timestamps"] = "#(FIRST) 10\n";
    const auto table = conversation("dialog::timestamps");
    BOOST_REQUIRE(table);
    const auto first = CaptureStringTable(table.get());
    BOOST_TEST(GetStringTimeStamp(SetAbsStringTableIndex(first, 1)) == nullptr);
    BOOST_TEST(std::string(GetStringAddress(SetAbsStringTableIndex(first, 1))) == "Second");
    BOOST_TEST(GetStringTimeStamp(first) == nullptr);
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
    const auto first = CaptureStringTable(table.get());
    BOOST_TEST(std::string(GetStringAddress(first)) == expected);
    BOOST_TEST(std::string(GetStringAddress(SetAbsStringTableIndex(first, 1))) == "Second");
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
    const auto first = CaptureStringTable(table.get());
    BOOST_TEST(GetStringTableCount(first) == 2u);
    BOOST_TEST(std::string(GetStringAddress(first), GetStringLengthBin(first)) == std::string("one\0", 4));
    BOOST_TEST(std::string(GetStringAddress(SetAbsStringTableIndex(first, 1)), GetStringLengthBin(SetAbsStringTableIndex(first, 1))) == std::string("x\0y", 3));
    BOOST_TEST(liveStreams == 1);
}

extern "C" int string_table_c_round_trip(void);

BOOST_AUTO_TEST_CASE(string_table_c_api_and_pointer_bytes) {
    BOOST_TEST(string_table_c_round_trip() == 1);
}

BOOST_FIXTURE_TEST_CASE(string_table_metadata_names_and_navigation, ResourceFixture) {
    files["dialog"] = "#(DUPLICATE) first.ogg\nFirst\n#(DUPLICATE)\n\n#(LAST) last.ogg\nLast\n";
    const auto table = conversation("dialog:voice/");
    const auto first = CaptureStringTable(table.get());
    BOOST_REQUIRE(first);
    const auto last = SetAbsStringTableIndex(first, 2);
    BOOST_TEST(GetStringTableCount(first) == 3u);
    BOOST_TEST(SetAbsStringTableIndex(first, 7) == SetRelStringTableIndex(first, 1));
    BOOST_TEST(SetRelStringTableIndex(first, -1) == last);
    BOOST_TEST(SetRelStringTableIndex(first, -7) == last);
    BOOST_TEST(SetRelStringTableIndex(last, 1) == first);
    BOOST_TEST(ReleaseStringTable(last) == table.get());
    const auto named = GetStringByName(table.get(), "DUPLICATE");
    BOOST_REQUIRE(named);
    BOOST_TEST(GetStringTableIndex(named) == 3u);
    BOOST_TEST(SetRelStringTableIndex(named, 0) == first);
    BOOST_TEST(std::string(GetStringSoundClip(named)) == "voice/first.ogg");
    const auto blank = SetRelStringTableIndex(first, 1);
    BOOST_TEST(GetStringLengthBin(blank) == 1u);
    BOOST_TEST(GetStringLength(blank) == 0u);
    BOOST_TEST(GetStringSoundClip(blank) == nullptr);
    BOOST_TEST(GetStringTimeStamp(last) == nullptr);
    BOOST_TEST(GetStringByName(table.get(), "absent") == nullptr);
    // Keys are owned copies: the legacy mutable name view cannot corrupt the map.
    GetStringAddress(named)[0] = 'X';
    BOOST_TEST(GetStringByName(table.get(), "DUPLICATE") == named);
    BOOST_TEST(std::string(GetStringAddress(first)) == "First");
}

BOOST_FIXTURE_TEST_CASE(string_table_utf8_binary_lengths_and_stable_views, ResourceFixture) {
    files["text"] = "#(FIRST)\nA\xc2\xb0\n#(SECOND)\nSecond\n";
    const Table table(LoadStringTableFile(nullptr, "text"), FreeStringTable);
    BOOST_REQUIRE(table);
    const auto first = CaptureStringTable(table.get());
    const auto bytes = GetStringAddress(first);
    BOOST_TEST(GetStringLength(first) == 2u);
    BOOST_TEST(GetStringLengthBin(first) == 4u);
    BOOST_TEST(GetStringName(first) == nullptr);
    BOOST_TEST(GetStringByName(table.get(), "FIRST") == nullptr);
    for (int i = 0; i < 100; ++i) {
        BOOST_TEST(SetRelStringTableIndex(first, -2) == first);
        BOOST_TEST(GetStringAddress(first) == bytes);
    }
    bytes[0] = 'B';
    BOOST_TEST(std::string(GetStringAddress(first)) == "B\xc2\xb0");
    BOOST_TEST(_cur_resfile_name == nullptr);
}

BOOST_AUTO_TEST_CASE(string_table_empty_and_null_contracts) {
    const Table table(AllocStringTable(0), FreeStringTable);
    BOOST_TEST(CaptureStringTable(table.get()) == nullptr);
    BOOST_TEST(GetStringByName(table.get(), "missing") == nullptr);
    BOOST_TEST(SetAbsStringTableIndex(nullptr, 1) == nullptr);
    BOOST_TEST(SetRelStringTableIndex(nullptr, -1) == nullptr);
    BOOST_TEST(GetStringTableCount(nullptr) == 0u);
    BOOST_TEST(GetStringLength(nullptr) == 0u);
    BOOST_TEST(GetStringLengthBin(nullptr) == 0u);
    BOOST_TEST(GetStringAddress(nullptr) == nullptr);
    BOOST_TEST(GetStringName(nullptr) == nullptr);
    BOOST_TEST(GetStringSoundClip(nullptr) == nullptr);
    BOOST_TEST(GetStringTimeStamp(nullptr) == nullptr);
    BOOST_TEST(GetStringByName(nullptr, "missing") == nullptr);
    BOOST_TEST(DestroyStringTable(nullptr) == TRUE);
}

BOOST_FIXTURE_TEST_CASE(string_table_resource_types_destroy_complete_tables, ResourceFixture) {
    registeredDeleters.clear();
    BOOST_REQUIRE(InstallStringTableResType() == TRUE);
    BOOST_REQUIRE(registeredDeleters.size() == 3u);
    files["dialog"] = "#(ONE) one.ogg\nOne\n";
    files["time"] = "#(ONE) 10\n";
    for (const char *type : {"STRTAB", "BINTAB", "CONVERSATION"}) {
        auto table = conversation("dialog:voice/:time");
        const auto deleter = registeredDeleters.at(type);
        BOOST_REQUIRE(deleter);
        BOOST_TEST(deleter(table.release()) == TRUE);
    }
}

BOOST_FIXTURE_TEST_CASE(binary_table_rejects_truncated_counts_offsets_and_lengths, ResourceFixture) {
    // Each fixture has a valid legacy prefix but inconsistent table metadata.
    for (const auto &words : {std::vector<unsigned>{0xffffffffu, 100u, 0u},
            {0xffffffffu, 0u, 100u}, {0xffffffffu, 1u, 0u, 10u}}) {
        std::string bytes;
        for (const auto word : words)
            for (int shift = 24; shift >= 0; shift -= 8)
                bytes += static_cast<char>((word >> shift) & 0xff);
        files["bad"] = bytes;
        const Stream stream(res_OpenResFile(nullptr, "bad", "rb"), res_CloseResFile);
        const Table table(static_cast<STRING_TABLE>(_GetBinaryTableData(stream.get(), bytes.size())), FreeStringTable);
        BOOST_TEST(!table);
    }
}

BOOST_FIXTURE_TEST_CASE(binary_table_preserves_empty_entries_and_word_offsets, ResourceFixture) {
    std::string bytes;
    for (const unsigned word : {0xffffffffu, 3u, 1u, 0u, 3u, 4u, 0xdeadbeefu})
        for (int shift = 24; shift >= 0; shift -= 8)
            bytes += static_cast<char>((word >> shift) & 0xff);
    bytes.append("a\0bnext", 7);
    files["binary"] = bytes;
    const Stream stream(res_OpenResFile(nullptr, "binary", "rb"), res_CloseResFile);
    const Table table(static_cast<STRING_TABLE>(_GetBinaryTableData(stream.get(), bytes.size())), FreeStringTable);
    BOOST_REQUIRE(table);
    const auto first = CaptureStringTable(table.get());
    BOOST_TEST(GetStringTableCount(first) == 3u);
    BOOST_TEST(GetStringAddress(first) == nullptr);
    BOOST_TEST(GetStringLength(first) == 0u);
    const auto second = SetRelStringTableIndex(first, 1);
    BOOST_TEST(GetStringLengthBin(second) == 3u);
    BOOST_TEST(std::string(GetStringAddress(second), 3) == std::string("a\0b", 3));
    const auto third = SetRelStringTableIndex(first, -1);
    BOOST_TEST(std::string(GetStringAddress(third), GetStringLengthBin(third)) == "next");
}

extern "C" int star_map_c_contract(void);

BOOST_FIXTURE_TEST_CASE(star_map_content_loader_closes_files_and_exposes_c_names, ResourceFixture) {
    FreeStarMap();
    BOOST_TEST(LoadStarMap() == FALSE); // Missing file.
    files["base/starmap.json"] = "{broken}";
    BOOST_TEST(LoadStarMap() == FALSE);
    BOOST_TEST(liveStreams == 0);
    std::ifstream input(UQM_TEST_STARMAP_PATH);
    BOOST_REQUIRE(input.good());
    files["base/starmap.json"].assign(std::istreambuf_iterator<char>(input), {});
    BOOST_REQUIRE(LoadStarMap() == TRUE);
    BOOST_TEST(liveStreams == 0);
    BOOST_TEST(star_map_c_contract() == 1);
    CurStarDescPtr = GetStar(0);
    FreeStarMap();
    BOOST_TEST(CurStarDescPtr == nullptr);
}

extern "C" int discovery_state_c_round_trip(void);
BOOST_FIXTURE_TEST_CASE(discoveries_import_legacy_reports_and_export_a_detached_save, ResourceFixture) {
    FreeStarMap();
    std::ifstream input(UQM_TEST_STARMAP_PATH);
    BOOST_REQUIRE(input.good());
    files["base/starmap.json"].assign(std::istreambuf_iterator<char>(input), {});
    BOOST_REQUIRE(LoadStarMap() == TRUE);
    BOOST_TEST(discovery_state_c_round_trip() == 1);
    FreeStarMap();
}
