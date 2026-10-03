#include "memtable/skip_list.hpp"

#include "keel.hpp"

#include <span>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("skip list can insert and delete a single element") {
    using namespace keel;
    SkipList skipList;
    uint8_t key[] = {0x0a, 0x0b, 0x0c, 0x0d}, value[] = {0x01, 0x02, 0x03};
    skipList.set(Key(key), Value(false, value));
    std::optional<Value> result = skipList.getOne(key);

    std::span<uint8_t> valueSpan = value;

    REQUIRE(result.has_value());
    REQUIRE(result.value().deleted == false);

    bool equal = std::equal(
        result.value().value.begin(), result.value().value.end(), 
        valueSpan.begin(), valueSpan.end()
    );
    REQUIRE(equal);
}