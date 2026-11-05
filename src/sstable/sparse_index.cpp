#include "sstable/sparse_index.hpp"
#include "cursor.hpp"
#include "util.hpp"
#include "sstable/sstable.hpp"
#include "types.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <stdexcept>

#include <system_error>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

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

    SparseIndex::SparseIndex(const char* dir, int id)
        : fd(0) {
        fd = open64(get_fname(dir, id, true).c_str(), O_RDONLY | O_DIRECT);
        fcntl(fd, F_SETFL, O_SYNC, 1);
        struct stat64 stat_result;
        int ret = stat64(get_fname(dir, id, true).c_str(), &stat_result);
        if (ret != 0) {
            throw_errno();
        }
        this->num_entries = static_cast<uint64_t>(stat_result.st_size) / 8;
    }

    uint64_t SparseIndex::getOffsetAtIndex(uint64_t idx) {
        uint64_t offset = 0;
        if (idx >= this->num_entries) {
            idx = this->num_entries - 1;
        }
        ssize_t ret = pread64(this->fd, &offset, 8, static_cast<off64_t>(8 * idx));
        if (ret == 0) {
            throw std::runtime_error("unexpected EOF");
        }
        if (ret < 0) {
            throw_errno();
        }
        return offset;
    }

    uint64_t SparseIndex::getNumEntries() {
        return this->num_entries;
    }

    SparseIndex::Generator::Generator(const char* dir, int id) : file(get_fname(dir, id, false)) {
        file.exceptions(std::ios::badbit | std::ios::eofbit);
    }

    void SparseIndex::Generator::append_offset(uint64_t offset) {
        file.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
    }

    void SparseIndex::Generator::close() {
        if (file.is_open()) {
            file.close();
        }
    }

    SparseIndex::Generator::~Generator() {
        this->close();
        printf("Closed FD for SparseIndex\n");
    }
}