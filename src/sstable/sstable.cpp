#include "sstable/sstable.hpp"

#include "types.hpp"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <stdexcept>

namespace keel {
    std::string SSTable::get_fname(const char* dir, int id, bool should_exist) {
        if (!std::filesystem::is_directory(dir)) {
            throw std::runtime_error("dir must be an actual directory");
        }
        std::string fname = dir;
        fname.append("/sstable_");
        fname.append(std::to_string(id));
        fname.append(".keelsst");
        verify_fname_exists(fname, should_exist);
        return fname;
    }

    SSTable::SSTable(const char* dir, int id)
    :file(get_fname(dir, id, true)) {
        this->file.exceptions(std::ios_base::failbit | std::ios_base::badbit);
    }

    std::optional<Value> SSTable::searchKey(Key key, off_t start_offset, size_t max_keys) { // NOLINT
        if (file.eof()) {
            return std::optional<Value>();
        }
        file.seekg(static_cast<off_t>(start_offset), std::ios_base::beg);
        for (size_t keys = 0; keys < max_keys; keys++) {
            Key read_key(file);
            if (file.eof()) {
                return std::optional<Value>();
            }
            if (compare_keys(read_key, key) == 0) {
                key.free();
                Value result(file);
                if (file.eof()) {
                    throw std::runtime_error("unexpected EOF, expected value");
                }
                return result;
            }
            key.free();
        }
        return std::optional<Value>();
    }

    void SSTable::generate(Memtable *memtable, const char *dir, int id) {
        std::string fname = get_fname(dir, id, false);
        std::ofstream file(fname, std::ios_base::binary);
        file.exceptions(std::ios_base::failbit | std::ios_base::badbit);

        Cursor<const KVPair> *cursor = memtable->getAll();

        bool failed = false;
        while (!failed && cursor->hasNext()) {
            const KVPair kvpair = cursor->getNext();
            try {
                kvpair.getKey().serialize(file);
            } catch (const std::exception& e) {

            }
                kvpair.getValue().serialize(file);
            } catch (const std::exception& e) {
            }
            kvpair.free();
        }


        delete cursor;
        file.close();
    }

}