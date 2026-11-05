#include "cursor.hpp"
#include "memtable/skip_list.hpp"

#include "types.hpp"

#include <cstdio>
#include <span>

#include <catch2/catch_test_macros.hpp>
#include <unordered_map>
#include <map>
#include <set>

TEST_CASE("skip list can insert and get a single element") {
    using namespace keel;
    SkipList skipList;
    uint8_t key[] = {0x0a, 0x0b, 0x0c, 0x0d}, value[] = {0x01, 0x02, 0x03};
    skipList.set(Key(key), Value(false, value));
    std::optional<Value> result = skipList.getOne(key);

    std::span<uint8_t> valueSpan = value;

    REQUIRE(result.has_value());

    if (result.has_value()) {
      REQUIRE(result.value().deleted == false);
    }

    if (result.has_value()) {
        REQUIRE(std::ranges::equal(result.value().value, valueSpan));
    }
}

TEST_CASE("skip list can insert and get multiple elements") {
    using namespace keel;
    const int TEST_SIZE = 400;

    SkipList skipList;

    for (uint32_t i = 0; i < TEST_SIZE; i++)  {
        std::span<uint8_t> tmp = std::span<uint8_t>(reinterpret_cast<uint8_t*>(&i), 4);
        skipList.set(Key(tmp.data(), tmp.size()), Value(false, tmp));
        REQUIRE(skipList.verifyInnerHasLower());
        //if (i%10 == 0 || i == TEST_SIZE-1) {
        //    // std::cout << "verified key " << i << " successfully\n";
        //}
    }

}

bool verify_sor_skiplist_parity(std::unordered_map<int, int> *sor, keel::SkipList *skipList, int KEY_COUNT) {
    for (int32_t i = 0; i < KEY_COUNT; i++) {
        keel::Key key(reinterpret_cast<uint8_t*>(&i), 4);
        std::optional<int> sor_result = (sor->contains(i)) ? sor->at(i) : std::optional<int>();
        std::optional<keel::Value> sl_result = skipList->getOne(key);
        if (sor_result.has_value()) {
            if (!sl_result.has_value()) {
                return false;
            }
            if (sor_result.value() != *(reinterpret_cast<const int32_t*>(sl_result.value().value.data()))) {
                return false;
            }
        } else {
            if (sl_result.has_value()) {
                return false;
            }
        }
    }
    return true;
}

TEST_CASE("skip list random inserts and deleted") {
    using namespace keel;

    const int KEY_COUNT = 100;
    const int OPERATIONS = 1000;

    std::default_random_engine prng(1);

    std::unordered_map<int, int> sor;
    SkipList skipList;

    for (int i = 0; i < OPERATIONS; i++) {
        int32_t op_key = static_cast<int32_t>(prng()) % KEY_COUNT;
        int32_t op_value = static_cast<int32_t>(prng());
        Key key(reinterpret_cast<uint8_t*>(&op_key), 4);
        Value value(false, std::span(reinterpret_cast<uint8_t*>(&op_value), 4));

        // std::cout << "Operation " << i << " key: ";
        // keel::print_key(key);
        // std::cout << " value: " << op_value << "\n";

        sor[op_key] = op_value;
        skipList.set(key, value);

        REQUIRE(skipList.verifyInnerHasLower());

        REQUIRE(verify_sor_skiplist_parity(&sor, &skipList, KEY_COUNT));
    }
}

TEST_CASE("skip list verify consistent range query") {
    using namespace keel;

    uint8_t keys[][1] = {
        {0x01},
        {0x02},
        {0x03},
    };
    uint8_t valid_value = 0x01;
    uint8_t invalid_value = 0x00;

    SkipList skipList;
    skipList.set(Key(keys[0], 1), Value(false, std::span<uint8_t>(&valid_value, 1)));
    skipList.set(Key(keys[1], 1), Value(false, std::span<uint8_t>(&valid_value, 1)));

    printf("Crated skip list with 2 keys\n");

    Cursor<const KVPair>* cursor = skipList.getAll();

    printf("Got cursor for range query\n");

    skipList.set(Key(keys[1], 1), Value(false, std::span<uint8_t>(&invalid_value, 1)));
    skipList.set(Key(keys[2], 1), Value(false, std::span<uint8_t>(&invalid_value, 1)));

    printf("Set values post-query\n");

    std::optional<KVPair> kv0 = cursor->getNext();
    std::optional<KVPair> kv1 = cursor->getNext();
    std::optional<KVPair> kv2 = cursor->getNext();

    printf("Got values\n");

    REQUIRE(kv0.has_value());
    REQUIRE(kv1.has_value());
    REQUIRE(!kv2.has_value());

    printf("Asserted values exist\n");

    assert(kv0.has_value());
    Value kv0Val = kv0->getValue();

    assert(kv1.has_value());
    Value kv1Val = kv1->getValue();

    REQUIRE((kv0Val.value[0] == valid_value && kv0Val.value.size() == 1));
    REQUIRE((kv1Val.value[0] == valid_value && kv1Val.value.size() == 1));

    printf("Assered values are right\n");

    delete cursor;

    printf("All done\n");
}

void genRandomSkipList(keel::SkipList *skipList, std::map<std::vector<uint8_t>, std::vector<uint8_t>> *sor, size_t n_elements) {
    std::independent_bits_engine<std::mt19937, CHAR_BIT, uint8_t> engine;
    for (size_t i = 0; i < n_elements; i++) {
        size_t keyLen = engine() % 20 + 10;
        uint8_t *keyBuf = new uint8_t[keyLen];
        keel::Key key(keyBuf, keyLen);
        size_t valLen = engine() % 20 + 10;
        uint8_t *valBuf = new uint8_t[valLen];
        keel::Value value(false, std::span<uint8_t>(valBuf, valLen));

        for (size_t j = 0; j < keyLen; j++) {
            keyBuf[j] = engine();
        }
        for (size_t j = 0; j < valLen; j++) {
            valBuf[j] = engine();
        }

        skipList->set(key, value);
        sor->insert_or_assign(std::vector<uint8_t>(key.begin(), key.end()), 
            std::vector<uint8_t>(value.value.begin(), value.value.end()));
        key.free();
        value.free();
    }
}

TEST_CASE("skip list verify getAll") {
    keel::SkipList sl;
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> sor;
    std::set<std::vector<uint8_t>> done;
    const int TEST_SIZE = 1000;

    genRandomSkipList(&sl, &sor, TEST_SIZE);

    keel::Cursor<const keel::KVPair> *cursor = sl.getAll();

    int result_count = 0;

    while (std::optional<const keel::KVPair> result = cursor->getNext()) {
        result_count++;

        keel::Key key = result->getKey();
        std::vector<uint8_t> keyVec(key.begin(), key.end());
        REQUIRE((!done.contains(keyVec)));
        done.insert(keyVec);

        std::vector<uint8_t> sorValue = sor[std::vector<uint8_t>(key.begin(), key.end())];
        keel::Value slValue = result->getValue();
        REQUIRE(std::equal(sorValue.begin(), sorValue.end(), slValue.value.begin(), slValue.value.end()));
    }

    REQUIRE(TEST_SIZE == result_count);
    delete cursor;
}