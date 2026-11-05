#pragma once


#include "sparse_index.hpp"

#include "cursor.hpp"
#include "memtable/memtable.hpp"
#include "types.hpp"

namespace keel {

    class SSTable {
    private:
        int fd;
    public:
        SSTable(const char* dir, int id);
        std::optional<Value> searchKey(Key key, off_t start_offset, size_t max_keys); // NOLINT
        static std::string get_fname(const char*dir, int id, bool should_exist);
        static int32_t generate(Memtable *memtable, const char* dir, int id);

        SparseIndex index;

        class SSTCursor: public SkippableCursor<keel::KVPair> {
        private:
            keel::Key upper_bound;
            size_t max_keys;
        public:
            SSTCursor() = delete;
            bool skipNext() override;
            std::optional<KVPair> getLast() override;
            std::optional<const keel::KVPair> getNext() override;
            ~SSTCursor();
        };

        std::optional<Value> getOne(keel::Key key);
        SSTCursor* getRange(keel::Key low, keel::Key high);
        SSTCursor* getAll();

        std::optional<Value> linearSearchKey(Key key, off64_t start_offset, size_t max_keys);

        static bool readAtOffset(int fd, uint8_t* buf, uint64_t n_bytes, uint64_t offset);

        std::optional<off64_t> searchKeyChunkOffset(keel::Key key); //binary search the sparse index and then linear search, to find the offset for given key.

    };
}