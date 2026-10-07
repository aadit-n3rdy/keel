
#include "catch2/catch_test_macros.hpp"
#include "keel.hpp"
#include "memtable/memtable.hpp"
#include "memtable/skip_list.hpp"
#include "sstable/sstable.hpp"

TEST_CASE("sstable serialize and deserialize test") {
    keel::SkipList skipList;
    keel::SSTable::generate(Memtable *memtable, const char *dir, int id);
}