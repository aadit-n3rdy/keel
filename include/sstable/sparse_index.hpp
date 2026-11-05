#pragma once

#include "types.hpp"

#include <cstdio>
#include <vector>
#include <fstream>

namespace keel {
    const int SSTABLE_CHUNK_SIZE = 100;
    class SparseIndex {
       int fd;
       uint64_t num_entries;

    public:
        SparseIndex(const char* dir, int id); // loads sparse index file
        static std::string get_fname(const char* dir, int id, bool should_exist);
        uint64_t getOffsetAtIndex(uint64_t idx);
        uint64_t getNumEntries();

        class Generator {
        private:
            std::ofstream file;
        public:
            Generator(const char* dir, int id);
            void append_offset(uint64_t offset);
            void close();
            ~Generator();
        };
    };
}