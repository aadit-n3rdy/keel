#pragma once

#include "types.hpp"

#include <iterator>
#include <vector>

namespace keel {
    const int SSTABLE_CHUNK_SIZE = 100;
    class SparseIndex {
    private:
        class Entry {
        public:
            Key key;
            off_t offset;
            inline void free() const {
                key.free();
            }
        };
        const std::vector<Entry> entries;
        static std::vector<SparseIndex::Entry> loadSparseIndexEntried(const char* dir, int id);

        class SSTIterator {
            // used exclusively to read list of entries from file and populate the vector
        private:
            std::ifstream* file;
            Entry cur_entry;
            void skipNextEntry();
            void storeNextEntry();
        public:
            using iterator_category  = std::input_iterator_tag;
            using value_type = Entry;
            using difference_type = std::ptrdiff_t;
            using pointer = const Entry*;
            using reference = const Entry&;


            explicit inline SSTIterator(std::ifstream* p_file) : file(p_file) {};
            reference operator*() const;
            pointer operator->() const;

            SSTIterator& operator++();
            SSTIterator operator++(int);

            bool operator==(const SSTIterator& other) const;

            bool operator!=(const SSTIterator& other) const;

            static SSTIterator begin(const char* fname);
            static SSTIterator end();

        };

        class Iterator {
            // used exclusively to read list of entries from file and populate the vector
        private:
            std::ifstream* file;
            Entry cur_entry;
            void storeNextEntry();
        public:
            using iterator_category  = std::input_iterator_tag;
            using value_type = Entry;
            using difference_type = std::ptrdiff_t;
            using pointer = const Entry*;
            using reference = const Entry&;


            explicit inline Iterator(std::ifstream* p_file) : file(p_file) {};
            reference operator*() const;
            pointer operator->() const;

            Iterator& operator++();
            Iterator operator++(int);

            bool operator==(const Iterator& other) const;

            bool operator!=(const Iterator& other) const;

            static Iterator begin(const char* fname);
            static Iterator end();

        };

    public:
        SparseIndex(const char* dir, int id); // loads sparse index file
        static void generate(const char* dir, int id); // writes a sparse index file based on the SSTable
        void free() const& ;
        static std::string get_fname(const char* dir, int id, bool should_exist);
    };
}