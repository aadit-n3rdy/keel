#include "sstable/sstable.hpp"

#include "constants.hpp"
#include "sstable/sparse_index.hpp"
#include "types.hpp"
#include "util.hpp"

#include <cassert>
#include <cstring>
#include <cstdio>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <sys/types.h>
#include <unistd.h>

namespace keel {
std::string SSTable::get_fname(const char *dir, int id, bool should_exist) {
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

SSTable::SSTable(const char *dir, int id) : fd(0), index(dir, id) {
    std::string fname = get_fname(dir, id, true);
    fd = open64(fname.c_str(), O_RDONLY);
    if (fd < 0) {
        throw_errno();
    }
}

bool SSTable::readAtOffset(int fd, uint8_t *buf, uint64_t n_bytes,
                           uint64_t offset) {
    // Wrapper over pread64, returns true iff EOF
    ssize_t ret = pread64(fd, buf, n_bytes, static_cast<off64_t>(offset));
    if (ret == 0) {
        return true;
    } else if (ret < 0) {
        throw_errno();
        // unreachable, since throw_errno throws
        return false;
    } else {
        return false;
    }
}

uint64_t align(uint64_t x, uint64_t boundary) {
    return x + (boundary - (x % boundary));
}

std::optional<Value> SSTable::linearSearchKey(Key key, off64_t start_offset,
                                              size_t max_keys) { // NOLINT
    uint64_t offset = static_cast<uint64_t>(start_offset);
    bool eof = false;
    std::vector<uint8_t> keyBuf;
    uint8_t *valBuf = NULL;
    std::optional<Value> result;
    int attempts = 0;
    bool done = false;
    size_t keys;
    for (keys = 0; !done && keys < max_keys; keys++, attempts++) {
        uint32_t keyLen = 0;
        printf("DEBUG: reading key length at offset %lu\n", offset);
        eof = readAtOffset(fd, reinterpret_cast<uint8_t *>(&keyLen), 4, offset);
        printf("DEBUG: found key length %u\n", keyLen);
        offset += 4;
        if (eof) {
            printf("Reached EOF, no more keys to search\n");
            done = true;
            break;
        }
        if (keyLen == 0) {
            offset -= 4;
            printf("DEBUG: reached page boundary \n");
            printf("DEBUG: current page: %lu\n", (offset & SSTABLE_PAGE_BITS));
            printf("DEBUG: nextt page: %lu\n", (offset & SSTABLE_PAGE_BITS) + SSTABLE_PAGE_SIZE);
            offset = (offset & SSTABLE_PAGE_BITS) + SSTABLE_PAGE_SIZE;
            printf(
                "DEBUG: reached page boundary, reading keyLen at offset %lu\n",
                offset);
            eof = readAtOffset(fd, reinterpret_cast<uint8_t *>(&keyLen), 4,
                               offset);
            printf("DEBUG: key length %u\n", keyLen);
            offset += 4;
            if (eof) {
                printf(
                    "Reached EOF at page boundary, no more keys to search\n");
                done = true;
                break;
            }
        }
        keyBuf.resize(keyLen);
        printf("DEBUG: Reading key at offset %lu\n", offset);
        eof = readAtOffset(fd, keyBuf.data(), keyLen, offset);
        offset += keyLen;
        if (eof) {
            printf("ERROR: unexpected EOF while trying to read key of "
                   "length %u "
                   "at offset %lu\n",
                   keyLen, offset - keyLen);
            throw std::runtime_error(
                "unexpected EOF while trying to read key value");
        }
        offset = align(offset, 4);

        Key read_key(keyBuf.data(), keyLen);
        int cmp_result = compare_keys(read_key, key);
        printf("DEBUGT: compare result: %d\n", cmp_result);
        uint32_t valLen = 0;
        printf("DEBUG: reading value length at offset %lu\n", offset);
        eof = readAtOffset(fd, reinterpret_cast<uint8_t *>(&valLen), 4, offset);
        printf("DEBUG: Read value length %u\n", valLen);
        offset += 4;
        if (eof) {
            printf("ERROR: unexpected EOF while trying to valLen at "
                   "offset %lu\n",
                   offset - 4);
            throw std::runtime_error(
                "unexpected EOF while attempting to read value length");
        }
        if (valLen == 0) {
            printf("WARN: value length zero at offset %lu\n", offset - 4);
        }
        if (cmp_result == 0) {
            printf("Linear search done with %d offsets attempted, "
                   "result is "
                   "at offset %lu\n",
                   attempts, offset - 4);
            done = true;
            if (valLen == 0) {
                printf("WARN: valLen is 0\n");
                result = Value(true, std::span<uint8_t>());
            } else {
                valBuf = new uint8_t[valLen];
                printf("DEBUG: reading value of length %u at offset %lu\n",
                       valLen, offset);
                eof = readAtOffset(fd, valBuf, valLen, offset);
                offset += valLen;
                if (eof) {
                    printf("ERROR: unexpected EOF while trying to "
                           "value of "
                           "size %u "
                           "at offset %lu\n",
                           valLen, offset - valLen);
                    printf("ERROR:unexpected EOF while reading value\n");
                } else {
                    result = Value(false, std::span<uint8_t>(valBuf, valLen));
                }
            }
        } else if (cmp_result > 0) {
            printf("WARN: went past boundary, result not found");
            done = true;
        } else {
            offset += valLen;
            offset = align(offset, 4);
        }
    }
    if (keys == max_keys) {
        printf("WARN: exceeded max keys search limit in linear search\n");
    }
    return result;
}

int32_t SSTable::generate(Memtable *memtable, const char *dir, int id) {
    // TODO: error handling
    std::string fname = get_fname(dir, id, false);
    int records_written = 0;
    Cursor<const KVPair> *cursor = memtable->getAll();

    int fd = open64(fname.c_str(), O_CREAT | O_WRONLY | O_DIRECT, 0644);

    uint8_t *pageBuf = new uint8_t[SSTABLE_PAGE_SIZE];

    uint64_t page_offset = 0;
    uint64_t offset = 0;
    bool wrote_last_offset = false;
    uint64_t offnum = 0;
    uint64_t file_offset = page_offset + offset;

    SparseIndex::Generator spgen(dir, id);

    ssize_t retval;

    while (const std::optional<const KVPair> result = cursor->getNext()) {
        try {
            file_offset = page_offset + offset;
            printf("file offset: %lu, page offset: %lu, offset: %lu\n", file_offset, page_offset, offset);
            wrote_last_offset = false;
            const KVPair &kvpair = result.value();
            Key key = kvpair.getKey();
            Value value = kvpair.getValue();
            uint64_t key_size = 4 + key.size();
            key_size = align(key_size, 4);
            uint64_t value_size = 4 + (value.deleted ? 0 : value.value.size());
            value_size = align(value_size, 4);

            uint64_t next_offset = (offset + key_size + value_size);

            if (next_offset >= SSTABLE_PAGE_SIZE) {
                // skip bytes iff at page boundary and not contiguous
                memcpy(pageBuf + offset, "\0\0\0\0", 4);

                printf("INFO: flushing page %lu\n", page_offset);
                retval = pwrite64(fd, pageBuf, SSTABLE_PAGE_SIZE, __off64_t(page_offset));
                if (retval < 0) {
                    delete[](pageBuf);
                    throw_errno();
                }

                offset = 0;
                page_offset += SSTABLE_PAGE_SIZE;
                file_offset = page_offset + offset;
                next_offset = offset + key_size + value_size;
            }
            uint64_t key_offset = offset;
            uint64_t value_offset =
                offset + key_size; // key_size includes alignment bytes
            if (offnum % SSTABLE_SPINDEX_CHUNK_SIZE == 0) {
                spgen.append_offset(file_offset);
                wrote_last_offset = true;
            }
            uint32_t sizebuf = uint32_t(key.size());
            // retval = pwrite64(fd, &sizebuf, 4, __off64_t(key_offset));
            memcpy(pageBuf + key_offset, &sizebuf, 4);
            // if (retval < 0) {
            //     throw_errno();
            // }
            // retval =
            //     pwrite64(fd, key.data(), key.size(), __off64_t(key_offset + 4));
            memcpy(pageBuf + key_offset + 4, key.data(), key.size());
            // if (retval < 0) {
            //     throw_errno();
            // }

            sizebuf = (value.deleted) ? 0 : uint32_t(value.value.size());
            // retval = pwrite64(fd, &sizebuf, 4, __off64_t(value_offset));
            memcpy(pageBuf + value_offset, &sizebuf, 4);
            // if (retval < 0) {
            //     throw_errno();
            // }
            if (!value.deleted) {
                // retval = pwrite64(fd, value.value.data(), value.value.size(),
                //                   __off64_t(value_offset + 4));
                memcpy(pageBuf + value_offset + 4, value.value.data(), value.value.size());
                // if (retval < 0) {
                //     throw_errno();
                // }
            }
            printf("Wrote offnum %d at offset %d\n", int(offnum), int(offset));

            offnum++;
            records_written++;
            offset = next_offset;
            printf("Next offset was %d", int(next_offset));
        } catch (std::exception &e) {
            close(fd);
            delete[](pageBuf);
            throw e;
        }
    }
    if (!wrote_last_offset) {
        spgen.append_offset(file_offset);
    }
    if (offset + 4 < SSTABLE_PAGE_SIZE) {
        memcpy(pageBuf + offset, "\0\0\0\0", 4); // to ensure that search does not read this as a key size
        retval =
            pwrite64(fd, pageBuf, SSTABLE_PAGE_SIZE, __off64_t(page_offset));
        if (retval < 0) {
            delete[] pageBuf;
            throw_errno();
        }
    }
    delete[] pageBuf;
    close(fd);
    printf("Closed FD for SSTable\n");
    delete cursor;
    return records_written;
}

keel::Key getKeyAtOffset(int fd, uint64_t offset) {
    uint32_t keySize;
    ssize_t ret = pread64(fd, &keySize, sizeof(keySize), off64_t(offset));
    if (ret == 0) {
        throw std::runtime_error("unexpected EOF");
    } else if (ret < 0) {
        throw_errno();
    }
    uint8_t *keyBuf = new uint8_t[keySize];
    ret = pread64(fd, keyBuf, keySize, off64_t(offset + 4));
    if (ret < 0) {
        throw_errno();
    } else if (ret == 0) {
        throw std::runtime_error("unexpected EOF");
    }
    return keel::Key(keyBuf, keySize);
}

keel::Value getValueAtKeyOffset(int fd, uint64_t offset) {
    uint32_t keySize;
    ssize_t ret = pread64(fd, &keySize, sizeof(keySize), off64_t(offset));
    if (ret == 0) {
        throw std::runtime_error("unexpected EOF");
    } else if (ret < 0) {
        throw_errno();
    }
    offset += 4 + keySize;
    offset = align(offset, 4);
    uint32_t valSize;
    ret = pread64(fd, &valSize, sizeof(valSize), off64_t(offset));
    if (ret == 0) {
        throw std::runtime_error("unexpected EOF");
    } else if (ret < 0) {
        throw_errno();
    }
    if (valSize == 0) {
        return keel::Value(true, std::span<uint8_t>());
    }
    uint8_t *valBuf = new uint8_t[valSize];
    ret = pread64(fd, valBuf, valSize, off64_t(offset));
    if (ret == 0) {
        throw std::runtime_error("unexpected EOF");
    } else if (ret < 0) {
        throw_errno();
    }
    return keel::Value(false, std::span<uint8_t>(valBuf, valSize));
}

std::optional<off64_t> SSTable::searchKeyChunkOffset(keel::Key key) {
    // returns the highest offset less than or equal to this key's length
    uint64_t ilo = 0,
             ihi = this->index.getNumEntries() - 1; // index low, and index high
    uint64_t olo = this->index.getOffsetAtIndex(0),
             ohi = this->index.getOffsetAtIndex(ihi);
    keel::Key klo = getKeyAtOffset(this->fd, olo);
    keel::Key khi;
    int clo, chi;
    clo = compare_keys(key, klo);
    if (clo < 0) {
        klo.free();
        printf("WARN: key was smaller than lowest\n");
        return std::optional<off64_t>();
    } else if (clo == 0) {
        klo.free();
        return olo;
    }
    klo.free();

    khi = getKeyAtOffset(this->fd, ohi);
    chi = compare_keys(key, khi);
    if (chi > 0) {
        khi.free();
        printf("WARN: key was greater than highest\n");
        return std::optional<off64_t>();
    } else if (chi == 0) {
        khi.free();
        return ohi;
    }
    khi.free();

    uint64_t imid, omid;
    keel::Key kmid;
    while (ilo < ihi) {
        printf("ilo: %lu, ihi: %lu\n", ilo, ihi);
        imid = (ilo + ihi) / 2;
        omid = this->index.getOffsetAtIndex(imid);
        kmid = getKeyAtOffset(this->fd, omid);
        int cmp_result = compare_keys(key, kmid);
        kmid.free();
        if (cmp_result == 0) {
            ilo = imid;
            break;
        } else if (cmp_result < 0) {
            assert(ihi - ilo > 1);
            ihi = imid - 1;
        } else {
            if (ihi - ilo == 1) {
                ohi = this->index.getOffsetAtIndex(ihi);
                khi = getKeyAtOffset(this->fd, ohi);
                cmp_result = compare_keys(key, khi);
                khi.free();
                if (cmp_result < 0) {
                    // ilo is correct
                } else {
                    ilo = ihi;
                }
                break;
            } else {
                ilo = imid;
            }
        }
    }
    return this->index.getOffsetAtIndex(ilo);
}

std::optional<Value> SSTable::getOne(keel::Key key) {
    printf("Binary searching index\n");
    std::optional<off64_t> binSearchResult = this->searchKeyChunkOffset(key);
    if (!binSearchResult.has_value()) {
        printf("WARN: no result from binary search\n");
        return std::optional<Value>();
    }
    printf("Found offset %lu in index\n", binSearchResult.value_or(0));
    off64_t offset = binSearchResult.value_or(
        0); // redundant, since it anyway will have a value
    printf("Starting linear search...\n");
    return linearSearchKey(key, offset, SSTABLE_SPINDEX_CHUNK_SIZE);
}
} // namespace keel