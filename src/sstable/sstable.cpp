#include "sstable/sstable.hpp"

#include "constants.hpp"
#include "sstable/sparse_index.hpp"
#include "types.hpp"
#include "util.hpp"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <sys/types.h>
#include <unistd.h>

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
    :fd(0), index(dir, id) {
        std::string fname = get_fname(dir, id, true);
        fd = open64(fname.c_str(), O_RDONLY);
        if (fd < 0) {
            throw_errno();
        }
    }

    bool SSTable::readAtOffset(int fd, uint8_t* buf, uint64_t n_bytes, uint64_t offset) {
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

    std::optional<Value> SSTable::linearSearchKey(Key key, off64_t start_offset, size_t max_keys) { // NOLINT
        uint64_t offset = static_cast<uint64_t>(start_offset);
        bool eof = false;
        std::vector<uint8_t> keyBuf;
        uint8_t* valBuf = NULL;
        std::optional<Value> result;
        for (size_t keys = 0; keys < max_keys; keys++) {
            uint32_t keyLen = 0;
            eof = readAtOffset(fd, reinterpret_cast<uint8_t*>(&keyLen), 4, offset);
            offset += 4;
            if (eof) {
                break;
            }
            if (keyLen == 0) {
                offset = (offset & SSTABLE_PAGE_BITS) + SSTABLE_PAGE_SIZE;
                eof = readAtOffset(fd, reinterpret_cast<uint8_t*>(&keyLen), 4, offset);
                offset += 4;
                if (eof) {
                    break;
                }
            }
            keyBuf.resize(keyLen);
            eof = readAtOffset(fd, keyBuf.data(), keyLen, offset);
            offset += keyLen;
            if (eof) {
                break;
            }
            if (offset % 4) {
                offset += 4-(offset % 4);
            }

            Key read_key(keyBuf.data(), keyLen);
            int cmp_result = compare_keys(read_key, key);
            uint32_t valLen = 0 ;
            eof = readAtOffset(fd, reinterpret_cast<uint8_t*>(&valLen), 4, offset);
            offset += 4;
            if (eof) {
                break;
            }
            if (cmp_result == 0) {
                if (valLen == 0) {
                    result = Value(true, std::span<uint8_t>());
                } else {
                    valBuf = new uint8_t[valLen];
                    eof = readAtOffset(fd, valBuf, valLen, offset);
                    offset += valLen;
                    if (eof) {
                        break;
                    }
                    result = Value(false, std::span<uint8_t>(valBuf, valLen));
                }
                break;
            } else if (cmp_result > 0) {
                break;
            }
        }
        return result;
    }

    int32_t SSTable::generate(Memtable *memtable, const char *dir, int id) {
        // TODO: error handling
        std::string fname = get_fname(dir, id, false);

        int records_written = 0;

        Cursor<const KVPair> *cursor = memtable->getAll();

        int fd = open64(fname.c_str(), O_CREAT | O_WRONLY, 0644);

        uint64_t offset = 0;
        bool wrote_last_offset = false;
        uint64_t last_offset = 0;
        uint64_t offnum = 0;

        SparseIndex::Generator spgen(dir, id);

        ssize_t retval;

        while (const std::optional<const KVPair> result = cursor->getNext()) {
          try {
            printf("offset is %d\n", int(offset));
            wrote_last_offset = false;
            const KVPair &kvpair = result.value();
            Key key = kvpair.getKey();
            Value value = kvpair.getValue();
            uint64_t key_size = 4 + key.size();
            if (key_size % 4 != 0) {
              key_size += 4 - (key_size % 4);
            }
            uint64_t value_size = 4 + (value.deleted ? 0 : value.value.size());
            if (value_size % 4 != 0) {
              value_size += 4 - (value_size % 4);
            }

            uint64_t offset_page = (offset & SSTABLE_PAGE_BITS);
            uint64_t next_offset = (offset + key_size + value_size);
            uint64_t next_offset_page = next_offset & SSTABLE_PAGE_BITS;

            if (offset_page != next_offset_page) {
              // skip bytes iff at page boundary and not contiguous
              retval = pwrite64(fd, "\0\0\0\0", 4, __off64_t(offset));
              if (retval < 0) {
                throw_errno();
              }
              offset = next_offset_page;
              next_offset = offset + key_size + value_size;
            }
            uint64_t key_offset = offset;
            uint64_t value_offset = offset + key_size; // key_size includes alignment bytes
            if (offnum % SSTABLE_SPINDEX_CHUNK_SIZE == 0) {
                spgen.append_offset(offset);
                wrote_last_offset = true;
            }
            uint32_t sizebuf = uint32_t(key.size());
            retval = pwrite64(fd, &sizebuf, 4, __off64_t(key_offset));
            if (retval < 0) {
                throw_errno();
            }
            retval = pwrite64(fd, key.data(), key.size(), __off64_t(key_offset + 4));
            if (retval < 0) {
                throw_errno();
            }

            sizebuf = (value.deleted) ? 0 : uint32_t(value.value.size());
            retval = pwrite64(fd, &sizebuf, 4, __off64_t(value_offset));
            if (retval < 0) {
                throw_errno();
            }
            if (!value.deleted) {
                retval = pwrite64(fd, value.value.data(), value.value.size(),
                       __off64_t(value_offset + 4));
                if (retval < 0) {
                    throw_errno();
                }
            }

            printf("Wrote offnum %d at offset %d\n", int(offnum), int(offset));

            offnum++;
            records_written++;
            last_offset = offset;
            offset = next_offset;
            printf("Next offset was %d", int(next_offset));
          } catch (std::exception &e) {
            close(fd);
            throw e;
          }
        }
        if (!wrote_last_offset) {
          spgen.append_offset(last_offset);
        }
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
        uint8_t* keyBuf = new uint8_t[keySize];
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
        if (offset % 4 != 0) {
            offset += 4 - (offset % 4);
        }
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
        uint64_t ilo = 0, ihi = this->index.getNumEntries()-1; // index low, and index high
        uint64_t olo = this->index.getOffsetAtIndex(0), ohi = this->index.getOffsetAtIndex(ihi);
        keel::Key klo = getKeyAtOffset(this->fd, olo);
        keel::Key khi;
        int clo, chi;
        clo = compare_keys(key, klo);
        if (clo < 0) {
            klo.free();
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
            return std::optional<off64_t>();
        } else if (chi == 0) {
            khi.free();
            return ohi;
        }
        khi.free();

        uint64_t imid, omid;
        keel::Key kmid;
        while (ilo < ihi) {
            imid = (ilo + ihi) / 2;
            omid = this->index.getOffsetAtIndex(imid);
            kmid = getKeyAtOffset(this->fd, omid);
            int cmp_result = compare_keys(key, kmid);
            if (cmp_result == 0) {
                kmid.free();
                return omid;
            } else if (cmp_result < 0) {
                kmid.free();
                ihi = imid-1;
            } else {
                kmid.free();
                ilo = imid;
            }
        }
        return ilo;
    }

    std::optional<Value> SSTable::getOne(keel::Key key) {
        std::optional<off64_t> binSearchResult = this->searchKeyChunkOffset(key);
        if (!binSearchResult.has_value()) {
            return std::optional<Value>();
        }
        off64_t offset = binSearchResult.value_or(0); // redundant, since it anyway will have a value
        return linearSearchKey(key, offset, SSTABLE_SPINDEX_CHUNK_SIZE);
    }
}