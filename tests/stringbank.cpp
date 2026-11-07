#include <boost/test/unit_test.hpp>

#include <array>
#include <memory>
#include <string>

#include "libs/resource/stringbank.h"

extern "C" int string_bank_c_round_trip(void);

namespace {
using Bank = std::unique_ptr<stringbank, decltype(&StringBank_Free)>;

/* Return a bank whose stored strings are released even if a test fails. */
Bank makeBank()
{
    return Bank(StringBank_Create(), StringBank_Free);
}
}

BOOST_AUTO_TEST_CASE(string_bank_copies_and_interns_source_text)
{
    const auto bank = makeBank();
    std::string source = "menu label";
    const char *label = StringBank_AddOrFindString(bank.get(), source.c_str());
    source.assign(4096, 'x');
    BOOST_TEST(std::string(label) == "menu label");
    BOOST_TEST(label == StringBank_AddOrFindString(bank.get(), "menu label"));
}

BOOST_AUTO_TEST_CASE(string_bank_keeps_short_and_long_pointers_stable_when_growing)
{
    constexpr int insertions = 2048;
    const auto bank = makeBank();
    const char *shortLabel = StringBank_AddOrFindString(bank.get(), "short");
    const std::string longSource(65536, 'L');
    const char *longLabel = StringBank_AddOrFindString(bank.get(), longSource.c_str());
    for (int i = 0; i < insertions; ++i) {
        const auto label = std::to_string(i);
        StringBank_AddOrFindString(bank.get(), label.c_str());
    }
    BOOST_TEST(std::string(shortLabel) == "short");
    BOOST_TEST(std::string(longLabel) == longSource);
    BOOST_TEST(shortLabel == StringBank_AddOrFindString(bank.get(), "short"));
    BOOST_TEST(longLabel == StringBank_AddOrFindString(bank.get(), longSource.c_str()));
}

BOOST_AUTO_TEST_CASE(string_bank_split_preserves_empty_fields_and_deduplicates)
{
    const auto bank = makeBank();
    std::array<const char *, 6> fields{};
    BOOST_REQUIRE(SplitString("\na\n\na\n", '\n', fields.size(), fields.data(), bank.get()) == 5);
    BOOST_TEST(std::string(fields[0]).empty());
    BOOST_TEST(std::string(fields[1]) == "a");
    BOOST_TEST(std::string(fields[2]).empty());
    BOOST_TEST(std::string(fields[3]) == "a");
    BOOST_TEST(std::string(fields[4]).empty());
    BOOST_TEST(fields[0] == fields[2]);
    BOOST_TEST(fields[1] == fields[3]);
    BOOST_TEST(fields[5] == nullptr);
}

BOOST_AUTO_TEST_CASE(string_bank_split_limits_output_and_retains_the_suffix)
{
    const auto bank = makeBank();
    const char *sentinel = "untouched";
    std::array<const char *, 3> fields{sentinel, sentinel, sentinel};
    BOOST_REQUIRE(SplitString("a,b,c,d", ',', 2, fields.data(), bank.get()) == 2);
    BOOST_TEST(std::string(fields[0]) == "a");
    BOOST_TEST(std::string(fields[1]) == "b,c,d");
    BOOST_TEST(fields[2] == sentinel);
    BOOST_REQUIRE(SplitString("a,b", ',', 1, fields.data(), bank.get()) == 1);
    BOOST_TEST(std::string(fields[0]) == "a,b");
    BOOST_TEST(SplitString("a,b", ',', 0, fields.data(), bank.get()) == 0);
    BOOST_TEST(std::string(fields[0]) == "a,b");
    BOOST_TEST(SplitString("a,b", ',', 0, nullptr, bank.get()) == 0);
}

BOOST_AUTO_TEST_CASE(string_bank_split_handles_empty_text_nul_delimiter_and_source_lifetime)
{
    const auto bank = makeBank();
    std::array<const char *, 2> fields{};
    BOOST_REQUIRE(SplitString("", '\n', fields.size(), fields.data(), bank.get()) == 1);
    BOOST_TEST(std::string(fields[0]).empty());
    BOOST_REQUIRE(SplitString("a,b", '\0', fields.size(), fields.data(), bank.get()) == 1);
    BOOST_TEST(std::string(fields[0]) == "a,b");
    {
        const std::string temporary = "first\nsecond";
        BOOST_REQUIRE(SplitString(temporary.c_str(), '\n', fields.size(), fields.data(), bank.get()) == 2);
    }
    BOOST_TEST(std::string(fields[0]) == "first");
    BOOST_TEST(std::string(fields[1]) == "second");
}

BOOST_AUTO_TEST_CASE(string_bank_api_is_callable_from_c)
{
    BOOST_TEST(string_bank_c_round_trip() == 1);
}
