#pragma once

#include "memtable/memtable.hpp"
#include "types.hpp"

#include <fstream>

namespace keel {

    class SSTable {
    private:
        std::ifstream file;
    public:
        SSTable(const char* dir, int id);
        std::optional<Value> searchKey(Key key, off_t start_offset, size_t max_keys); // NOLINT
        static std::string get_fname(const char*dir, int id, bool should_exist);
        static void generate(Memtable *memtable, const char* dir, int id);
    };
}