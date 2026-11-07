#include <boost/test/unit_test.hpp>

#include <memory>
#include <set>
#include <string>
#include <type_traits>

#include "libs/uio/charhashtable.h"

extern "C" int hash_table_c_round_trip(void);

namespace {
using Table = std::unique_ptr<CharHashTable_HashTable,
        decltype(&CharHashTable_deleteHashTable)>;
using Iterator = std::unique_ptr<CharHashTable_Iterator,
        decltype(&CharHashTable_freeIterator)>;

/* Return an owned empty table, including cleanup during failed assertions. */
Table makeTable()
{
    return Table(CharHashTable_newHashTable(), CharHashTable_deleteHashTable);
}

/* Borrow table during iteration; table must remain unchanged until destruction. */
Iterator iterate(const CharHashTable_HashTable &table)
{
    return Iterator(CharHashTable_getIterator(&table), CharHashTable_freeIterator);
}
}

static_assert(std::is_same_v<decltype(CharHashTable_iteratorKey(nullptr)), const char *>);

BOOST_AUTO_TEST_CASE(hash_table_empty_and_missing_keys)
{
    const auto table = makeTable();
    BOOST_TEST(CharHashTable_count(table.get()) == 0u);
    BOOST_TEST(CharHashTable_find(table.get(), "missing") == nullptr);
    BOOST_TEST(!CharHashTable_remove(table.get(), "missing"));
    const auto it = iterate(*table);
    BOOST_TEST(CharHashTable_iteratorDone(it.get()));
}

BOOST_AUTO_TEST_CASE(hash_table_owns_keys_and_borrows_values)
{
    int original = 7;
    int replacement = 9;
    auto table = makeTable();
    {
        std::string key = "name";
        BOOST_REQUIRE(CharHashTable_add(table.get(), key.c_str(), &original));
        key[0] = 'X';
    }
    BOOST_TEST(CharHashTable_find(table.get(), "name") == &original);
    BOOST_TEST(!CharHashTable_add(table.get(), "name", &replacement));
    BOOST_TEST(CharHashTable_find(table.get(), "name") == &original);
    BOOST_TEST(CharHashTable_count(table.get()) == 1u);
    BOOST_TEST(CharHashTable_remove(table.get(), "name"));
    BOOST_TEST(!CharHashTable_remove(table.get(), "name"));
    BOOST_REQUIRE(CharHashTable_add(table.get(), "name", &replacement));
    table.reset();
    BOOST_TEST(original == 7);
    BOOST_TEST(replacement == 9);
}

BOOST_AUTO_TEST_CASE(hash_table_null_values_and_empty_keys)
{
    const auto table = makeTable();
    BOOST_REQUIRE(CharHashTable_add(table.get(), "", nullptr));
    BOOST_TEST(!CharHashTable_add(table.get(), "", nullptr));
    BOOST_TEST(CharHashTable_count(table.get()) == 1u);
    const auto it = iterate(*table);
    BOOST_TEST(std::string(CharHashTable_iteratorKey(it.get())).empty());
    BOOST_TEST(CharHashTable_iteratorValue(it.get()) == nullptr);
}

BOOST_AUTO_TEST_CASE(hash_table_growth_removal_and_unordered_iteration)
{
    constexpr int entryCount = 2048;
    int borrowedValue = 17;
    const auto table = makeTable();
    for (int i = 0; i < entryCount; ++i) {
        const auto key = std::to_string(i);
        BOOST_REQUIRE(CharHashTable_add(table.get(), key.c_str(), &borrowedValue));
    }
    for (int i = 0; i < entryCount; i += 2) {
        const auto key = std::to_string(i);
        BOOST_REQUIRE(CharHashTable_remove(table.get(), key.c_str()));
    }
    std::set<std::string> visited;
    const auto it = iterate(*table);
    while (!CharHashTable_iteratorDone(it.get())) {
        BOOST_TEST(visited.insert(CharHashTable_iteratorKey(it.get())).second);
        BOOST_TEST(CharHashTable_iteratorValue(it.get()) == &borrowedValue);
        CharHashTable_iteratorNext(it.get());
    }
    BOOST_TEST(visited.size() == static_cast<size_t>(entryCount / 2));
    for (int i = 0; i < entryCount; ++i) {
        const auto key = std::to_string(i);
        BOOST_TEST((visited.count(key) != 0) == (i % 2 != 0));
        BOOST_TEST((CharHashTable_find(table.get(), key.c_str()) != nullptr) == (i % 2 != 0));
    }
}

BOOST_AUTO_TEST_CASE(hash_table_api_is_callable_from_c)
{
    BOOST_TEST(hash_table_c_round_trip() == 1);
}
