#include "libs/strings/strintrn.h"
#include "uqm/conversation_text.hpp"
#include <boost/test/unit_test.hpp>
namespace {
std::unique_ptr<string_table> textFixture() {
    std::vector<StringEntry> entries(2);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        entries[i].text.data = std::string("Translated original") + '\0';
        entries[i].name.data = std::string(i ? "ANSWER" : "HELLO") + '\0';
        entries[i].soundClip = std::string("voice.ogg") + '\0';
        entries[i].timeStamp = std::string("10,20") + '\0';
    }
    return MakeStringTable(std::move(entries));
}
uqm::dialogue::Json textDefinition() {
    using uqm::dialogue::textFingerprint;
    return {{"id", "test"},
            {"resource", "comm.test.dialogue"},
            {"phrases",
             {{{"index", 0},
               {"id", "HELLO"},
               {"text", "English original"},
               {"original_text_hash", textFingerprint("English original")}},
              {{"index", 1},
               {"id", "ANSWER"},
               {"text", "English answer"},
               {"original_text_hash", textFingerprint("English answer")}}}}};
}
} // namespace
BOOST_AUTO_TEST_CASE(
    live_json_edits_replace_text_while_preserving_translation_and_speech_metadata) {
    auto table = textFixture();
    const auto handle = CaptureStringTable(table.get());
    const auto *clip = GetStringSoundClip(handle);
    const auto *stamp = GetStringTimeStamp(handle);
    auto data = textDefinition();
    BOOST_TEST(uqm::applyConversationText(data, "comm.test.dialogue", handle) == 0u);
    BOOST_TEST(std::string(GetStringAddress(handle)) == "Translated original");
    data["phrases"][0]["text"] = "Poop jokes in space";
    BOOST_TEST(uqm::applyConversationText(data, "comm.test.dialogue", handle) == 1u);
    BOOST_TEST(std::string(GetStringAddress(handle)) == "Poop jokes in space");
    BOOST_CHECK(GetStringSoundClip(handle) == clip);
    BOOST_CHECK(GetStringTimeStamp(handle) == stamp);
    BOOST_TEST(std::string(GetStringName(handle)) == "HELLO");
    BOOST_TEST(std::string(GetStringAddress(SetAbsStringTableIndex(handle, 1))) ==
               "Translated original");
}
BOOST_AUTO_TEST_CASE(live_text_validates_all_phrase_names_before_importing_any_edit) {
    auto table = textFixture();
    auto data = textDefinition();
    data["phrases"][0]["text"] = "Changed";
    data["phrases"][1]["id"] = "UNKNOWN";
    BOOST_CHECK_THROW(
        uqm::applyConversationText(data, "comm.test.dialogue", CaptureStringTable(table.get())),
        std::invalid_argument);
    BOOST_TEST(std::string(GetStringAddress(CaptureStringTable(table.get()))) ==
               "Translated original");
}
