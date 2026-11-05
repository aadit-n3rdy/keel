
#include "catch2/catch_test_macros.hpp"
#include "memtable/skip_list.hpp"
#include "sstable/sstable.hpp"
#include "types.hpp"
#include <algorithm>
#include <filesystem>
#include <map>
#include <random>
#include <span>

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

TEST_CASE("sstable single entry serialize and deserialize test") {
    keel::SkipList skipList;
    std::vector<uint8_t> key({0x01, 0x02, 0x03});
    std::vector<uint8_t> value({0x0a, 0x0b});

    std::filesystem::remove("./sstable_1.keelsst");
    std::filesystem::remove("./sstable_1.keelspindex");

    skipList.set(key, keel::Value(false, value));
    
    keel::SSTable::generate(&skipList, ".", 1);

    keel::SSTable sst(".", 1);
    
    std::optional<keel::Value> result = sst.getOne(key);
    REQUIRE(result.has_value());
    keel::Value val = result.value_or(keel::Value(false, std::span<const uint8_t>()));
    REQUIRE((!val.deleted));
    REQUIRE(std::equal(val.value.begin(), val.value.end(), value.begin(), value.end()));
    val.free();
}

TEST_CASE("sstable serialize and deserialize test") {
    keel::SkipList skipList;
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> sor;
    const int N_ELEMENTS = 10;

    std::filesystem::remove("./sstable_123.keelsst");
    std::filesystem::remove("./sstable_123.keelspindex");

    genRandomSkipList(&skipList, &sor, N_ELEMENTS);

    printf("INFO: generated skip list of %d elements\n", N_ELEMENTS);

    int32_t records_written = keel::SSTable::generate( &skipList, "./", 123);
    REQUIRE(records_written == N_ELEMENTS);

    printf("INFO: Wrote SSTable and Sparse Index files\n");

    keel::SSTable sstable("./", 123);

    printf("INFO: Instantiated SSTable\n");

    for (auto it = sor.begin(); it != sor.end(); it++) {
        keel::Key key(it->first);
        keel::Value value(false, it->second);

        std::optional<keel::Value> retValue = sstable.getOne(key);
        REQUIRE(retValue.has_value());
        keel::Value readValue = retValue.value_or(keel::Value(true, std::span<const uint8_t>()));
        REQUIRE(std::equal(readValue.value.begin(), readValue.value.end(), value.value.begin(), value.value.end()));
        readValue.free();
    }

    printf("INFO: all keys matched against SOR\n");

    std::filesystem::remove("./sstable_123.keelsst");
}