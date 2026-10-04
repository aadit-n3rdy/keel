
#include "types.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
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