#include "sstable/sparse_index.hpp"
#include "sstable/sstable.hpp"

#include <filesystem>
#include <fstream>
#include <ios>
#include <stdexcept>

namespace keel {

    std::string SparseIndex::get_fname(const char* dir, int id, bool should_exist) {
        if (!std::filesystem::is_directory(dir)) {
            throw std::runtime_error("dir must be an actual directory");
        }
        std::string fname = dir;
        fname.append("/sstable_");
        fname.append(std::to_string(id));
        fname.append(".keelspindex");
        verify_fname_exists(fname, should_exist);
        return fname;
    }

    SparseIndex::SSTIterator SparseIndex::SSTIterator::begin(const char* fname) {
        std::ifstream* file = new std::basic_ifstream<char>(fname, std::ios_base::binary);
        file->exceptions(std::ios_base::badbit | std::ios_base::failbit);
        return SSTIterator(file);
    }

    SparseIndex::SSTIterator SparseIndex::SSTIterator::end() {
        return SSTIterator(nullptr);
    }

    SparseIndex::SSTIterator::reference SparseIndex::SSTIterator::operator*() const {
        if (file != nullptr && !file->eof()) {
            return cur_entry;
        } else {
            throw std::runtime_error("dereferenced already completed SparseIndex iterator");
        }
    }

    SparseIndex::SSTIterator::pointer SparseIndex::SSTIterator::operator->() const {
        if (file != nullptr && !file->eof()) {
            return &cur_entry;
        } else {
            throw std::runtime_error("dereferenced already completed SparseIndex iterator");
        }
    }

    void SparseIndex::SSTIterator::skipNextEntry() {
        Key::skip(*this->file);
        Value::skip(*this->file);
    }

    void SparseIndex::SSTIterator::storeNextEntry() {
        if (this->file->eof()) {
            return;
        }
        for (int skipped = 0; skipped < keel::SSTABLE_CHUNK_SIZE; skipped++) {
            skipNextEntry();
            if (this->file->eof()) {
                return;
            }
        }
        this->cur_entry.offset = static_cast<off_t>(this->file->tellg());
        this->cur_entry.key = Key(*this->file);
    }

    SparseIndex::SSTIterator& SparseIndex::SSTIterator::operator++() {
        this->storeNextEntry();
        return *this;
    }

    SparseIndex::SSTIterator SparseIndex::SSTIterator::operator++(int) {
        SSTIterator tmp = *this;
        this->storeNextEntry();
        return tmp;
    }

    SparseIndex::Iterator SparseIndex::Iterator::begin(const char* fname) {
        std::ifstream* file = new std::basic_ifstream<char>(fname, std::ios_base::binary);
        file->exceptions(std::ios_base::badbit | std::ios_base::failbit);
        return Iterator(file);
    }

    SparseIndex::Iterator SparseIndex::Iterator::end() {
        return Iterator(nullptr);
    }

    SparseIndex::Iterator::reference SparseIndex::Iterator::operator*() const {
        if (file != nullptr && !file->eof()) {
            return cur_entry;
        } else {
            throw std::runtime_error("dereferenced already completed SparseIndex iterator");
        }
    }

    SparseIndex::Iterator::pointer SparseIndex::Iterator::operator->() const {
        if (file != nullptr && !file->eof()) {
            return &cur_entry;
        } else {
            throw std::runtime_error("dereferenced already completed SparseIndex iterator");
        }
    }

    void SparseIndex::Iterator::storeNextEntry() {
        if (this->file->eof()) {
            return;
        }
        this->cur_entry.key = Key(*this->file);
        if (this->file->eof()) {
            throw std::runtime_error("unexpected EOF while reading sparse index");
        }
        this->file->read(reinterpret_cast<char*>(&this->cur_entry.offset), sizeof(cur_entry.offset));
        if (this->file->eof()) {
            throw std::runtime_error("unexpected EOF while reading sparse index offset");
        }
    }

    SparseIndex::Iterator& SparseIndex::Iterator::operator++() {
        this->storeNextEntry();
        return *this;
    }

    SparseIndex::Iterator SparseIndex::Iterator::operator++(int) {
        Iterator tmp = *this;
        this->storeNextEntry();
        return tmp;
    }

    SparseIndex::SparseIndex(const char* dir, int id)
        : entries(
            SparseIndex::Iterator::begin(SparseIndex::get_fname(dir, id, true).c_str()),
            SparseIndex::Iterator::end()
        ) {};

    void SparseIndex::generate(const char* dir, int id) {
        std::string sst_fname = SSTable::get_fname(dir, id, true);
        std::string sp_fname = SSTable::get_fname(dir, id, false);

        std::ofstream file(sp_fname, std::ios_base::binary);
        file.exceptions(std::ios_base::badbit | std::ios_base::failbit);

        SSTIterator begin = SSTIterator::begin(sst_fname.c_str());
        SSTIterator end = SSTIterator::end();
        for (auto entry = begin; entry != end; entry++) {
            entry->key.serialize(file);
            file.write(reinterpret_cast<const char*>(&entry->offset),  sizeof(entry->offset)); // TODO: fix endianness
            entry->free();
        }
        file.close();
    }

    void SparseIndex::free() const& {
        for (auto it = entries.begin(); it != entries.end(); it++) {
            it->free();
        }
    }
}