#include "uqm/gendef.h"
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <functional>
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
#include "uqm/game_state.hpp"
#include "uqm/arcade_session.hpp"
#include "uqm/uqm_context.hpp"
#include <nlohmann/json.hpp>
#include <type_traits>

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

std::function<void *(RESOURCE)> testResourceLookup;

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
void *res_GetResource(RESOURCE resource) { return testResourceLookup ? testResourceLookup(resource) : nullptr; }
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
    SetCurrentStar(GetStar(0));
    FreeStarMap();
    BOOST_TEST(GetCurrentStar() == nullptr);
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

extern "C" int system_behavior_bridge_contract(STAR_DESC *star);
BOOST_FIXTURE_TEST_CASE(system_behavior_bridge_uses_its_attached_campaign, ResourceFixture) {
    FreeStarMap();
    std::ifstream input(UQM_TEST_STARMAP_PATH);
    BOOST_REQUIRE(input.good());
    files["base/starmap.json"].assign(std::istreambuf_iterator<char>(input), {});
    BOOST_REQUIRE(LoadStarMap() == TRUE);
    STAR_DESC *chmmr = nullptr;
    for (COUNT i = 0; i < NUM_SOLAR_SYSTEMS; ++i)
        if (GetStar(i)->Index == CHMMR_DEFINED) { chmmr = GetStar(i); break; }
    BOOST_REQUIRE(chmmr);
    BOOST_TEST(system_behavior_bridge_contract(chmmr) == 1);
    FreeStarMap();
}


namespace {
std::string definitionFixture(const char *path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("missing definition fixture");
    return {std::istreambuf_iterator<char>(input), {}};
}
void stockDefinitions() {
    files["base/ships.json"] = definitionFixture(UQM_TEST_SHIPS_PATH);
    files["base/starmap.json"] = definitionFixture(UQM_TEST_STARMAP_PATH);
}
}
static_assert(std::is_same_v<decltype(std::declval<uqm::Session &>().definitions()), const uqm::GameDefinitions &>);
static_assert(std::is_same_v<decltype(std::declval<const uqm::GameDefinitions &>().ships()), const uqm::combat::ShipTuningCatalog &>);

BOOST_FIXTURE_TEST_CASE(gameplay_sessions_load_only_their_own_definitions, ResourceFixture) {
    stockDefinitions();
    uqm::UQM_Context context;
    uqm::SessionDispatcher host{context};
    uqm::GameSession campaign{host, uqm::GameSession::Entry::NewGame};
    BOOST_TEST(campaign.definitions().hasStars());
    BOOST_TEST(campaign.definitions().ships().at("arilou").ship.maxCrew == 6u);
    files.erase("base/starmap.json"); // Arcade creation must not open this file.
    uqm::ArcadeSession arcade{host};
    BOOST_TEST(!arcade.definitions().hasStars());
    BOOST_CHECK_THROW(arcade.definitions().stars(), std::logic_error);
    BOOST_TEST(arcade.definitions().ships().at("arilou").ship.maxCrew == 6u);
    BOOST_TEST(&campaign.definitions().ships() != &arcade.definitions().ships());
    BOOST_TEST(liveStreams == 0);
}

BOOST_FIXTURE_TEST_CASE(frontend_sessions_do_not_require_gameplay_definition_files, ResourceFixture) {
    struct Frontend final : uqm::Session {
        explicit Frontend(uqm::SessionDispatcher &host) : Session(host, RuntimeKind::Frontend, 1) {}
        uqm::SessionResult run() override { return uqm::DispatchCompleted{}; }
    };
    uqm::UQM_Context context;
    uqm::SessionDispatcher host{context};
    Frontend title{host};
    BOOST_CHECK_THROW(title.definitions(), std::logic_error);
    BOOST_TEST(liveStreams == 0);
}

BOOST_FIXTURE_TEST_CASE(failed_session_definition_loading_does_not_replace_an_existing_catalog, ResourceFixture) {
    stockDefinitions();
    uqm::UQM_Context context;
    uqm::SessionDispatcher host{context};
    uqm::GameSession original{host, uqm::GameSession::Entry::NewGame};
    files["base/starmap.json"] = "{broken}";
    BOOST_CHECK_THROW((uqm::GameSession{host, uqm::GameSession::Entry::NewGame}), std::exception);
    BOOST_TEST(original.definitions().hasStars());
    BOOST_TEST(original.definitions().ships().at("arilou").ship.maxCrew == 6u);
    BOOST_TEST(liveStreams == 0);
    files.erase("base/ships.json");
    BOOST_CHECK_THROW((uqm::ArcadeSession{host}), std::runtime_error);
    BOOST_TEST(liveStreams == 0);
}

BOOST_FIXTURE_TEST_CASE(c_adapters_follow_session_catalogs_and_player_star_identity, ResourceFixture) {
    stockDefinitions();
    struct Probe final : uqm::GameSession {
        uqm::SessionDispatcher &host;
        bool second;
        Probe(uqm::SessionDispatcher &host, bool second) : GameSession(host, Entry::NewGame), host(host), second(second) {}
        uqm::SessionResult run() override {
            BOOST_TEST(GetCurrentStar() == nullptr);
            BOOST_TEST(!player().navigationState().currentStar.has_value());
            BOOST_TEST(uqm::combat::shipTuning("arilou").ship.maxCrew == (second ? 17u : 6u));
            SetCurrentStar(GetStar(0));
            BOOST_REQUIRE(player().navigationState().currentStar.has_value());
            BOOST_TEST(player().navigationState().currentStar->value == 0u);
            BOOST_TEST(GetCurrentStar() == GetStar(0));
            BOOST_TEST(definitions().stars().at(StarId{0}).position().x == GetCurrentStar()->star_pt.x);
            if (!second) {
                auto document = nlohmann::json::parse(files.at("base/ships.json"));
                document["ships"]["arilou"]["max_crew"] = 17;
                files["base/ships.json"] = document.dump();
                auto next = std::make_unique<Probe>(host, true);
                BOOST_TEST(uqm::combat::shipTuning("arilou").ship.maxCrew == 6u);
                return next;
            }
            SetCurrentStar(nullptr);
            BOOST_TEST(!player().navigationState().currentStar.has_value());
            return uqm::DispatchCompleted{};
        }
    };
    uqm::UQM_Context context;
    uqm::SessionDispatcher host{context};
    const auto exit = host.run(std::make_unique<Probe>(host, false));
    BOOST_TEST(static_cast<int>(exit.kind) == static_cast<int>(uqm::ApplicationExitKind::Shutdown));
    BOOST_TEST(liveStreams == 0);
}
