
#include "types.hpp"
#include "wal.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>

TEST_CASE("serialize and deserialize var len fields") {
    uint8_t buf[] = {0x0a, 0x0b, 0x0c, 0x00, 0x0d};
    std::span<uint8_t> src_span(buf);

    std::ofstream ofile("test.bin");

    keel::write_varlen_bytes(ofile, src_span);

    REQUIRE(ofile.good());

    ofile.flush();
    ofile.close();
    
    std::ifstream ifile("test.bin");

    REQUIRE(ifile.good());

    std::span<uint8_t> target_span = keel::read_varlen_bytes(ifile);

    REQUIRE(ifile.good());
    ifile.close();

    REQUIRE(std::equal(src_span.begin(), src_span.end(), target_span.begin(), target_span.end()));

    delete[](target_span.data());
    std::remove("test.bin");
}

TEST_CASE("serialize and deserialize key") {
    uint8_t key_buf[] = { 0x0a, 0x0b, 0x0c, 0x00, 0x0d};
    keel::Key key(key_buf);

    const char *fname = "test_key_serde.bin";

    std::ofstream ofile(fname);

    key.serialize(ofile);

    REQUIRE(ofile.good());

    ofile.flush();
    ofile.close();
    
    std::ifstream ifile(fname);

    REQUIRE(ifile.good());

    keel::Key read_key(ifile);

    REQUIRE(ifile.good());
    ifile.close();

    REQUIRE(keel::compare_keys(key, read_key) == 0);

    read_key.free();
    std::remove(fname);
}

TEST_CASE("serialize and deserialize value") {
    uint8_t val_buf[] = { 0x0a, 0x0b, 0x0c, 0x00, 0x0d};
    keel::Value val(false, val_buf);

    const char *fname = "test_val_serde.bin";

    std::ofstream ofile(fname);

    val.serialize(ofile);

    REQUIRE(ofile.good());

    ofile.flush();
    ofile.close();
    
    std::ifstream ifile(fname);

    REQUIRE(ifile.good());

    keel::Value read_val(ifile);

    REQUIRE(ifile.good());
    ifile.close();

    REQUIRE(val.deleted == read_val.deleted);
    REQUIRE(read_val.deleted == false);
    REQUIRE(std::equal(val.value.begin(), val.value.end(), read_val.value.begin(), read_val.value.end()));

    read_val.free();
    std::remove(fname);
}

TEST_CASE("write to and read from WAL") {
    using Operation = keel::Wal::Operation;
    keel::Wal wal(".", 123);

    REQUIRE(std::filesystem::is_regular_file("./wal_123.keelwal"));

    uint8_t keys[4][4] = {
        {0x0a, 0x0b, 0x0c},
        {0x0a, 0x0d, 0x0e, 0x0f},
        {0x01, 0x02, 0x03},
        {0x00, 0x00, 0x00}
    };

    Operation operations[4] = {
        Operation::DELETE(keel::Key(keys[0])),
        Operation::SET(keel::Key(keys[1]), keel::Value(false, keys[1])),
        Operation::SET(keel::Key(keys[2]), keel::Value(true, keys[2])),
        Operation::SET(keel::Key(keys[3]), keel::Value(false, keys[3])),
    };

    wal.append_atomic(operations, operations+4);

    wal.close();

    keel::WalReader reader(".", 123);

    for (int i = 0; i < 4; i++) {
        std::optional<Operation> wal_entry = reader.read_next();
        REQUIRE(wal_entry.has_value());
        if (wal_entry.has_value()) {
            REQUIRE(wal_entry->type == operations[i].type);
            REQUIRE(keel::compare_keys(wal_entry->key, operations[i].key) == 0);
            if (operations[i].type != keel::Wal::OpType::DELETE) {
                REQUIRE(wal_entry.value().value.has_value());
                REQUIRE(operations[i].value.has_value());
                if (wal_entry.value().value.has_value() && operations[i].value.has_value()) {
                    REQUIRE(wal_entry->value->deleted == operations[i].value->deleted); // NOLINT
                    if (!operations[i].value->deleted) { // NOLINT
                        std::span<uint8_t> v1 = wal_entry->value->value; // NOLINT
                        std::span<uint8_t> v2 = operations[i].value->value; // NOLINT
                        REQUIRE(std::equal(v1.begin(), v1.end(), v2.begin(), v2.end()));
                    }
                }
            }
            wal_entry->free();
        }
    }

    std::optional<Operation> wal_entry = reader.read_next();
    REQUIRE(!wal_entry.has_value());

    std::filesystem::remove("./wal_123.keelwal");
}