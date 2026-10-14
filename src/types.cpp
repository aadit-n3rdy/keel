#include "types.hpp"
#include <cstdint>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <sys/types.h>
#include <filesystem>
#include <utility>

namespace keel {
    void verify_fname_exists(const std::string& fname, bool should_exist) {
        bool exists = std::filesystem::exists(fname);
        bool is_regular = std::filesystem::is_regular_file(fname);

        if (exists && !should_exist) {
            throw std::runtime_error("file already exists");
        } else if ((!exists || !is_regular) && should_exist) {
            throw std::runtime_error("is not a valid file");
        }
    }

    int compare_keys(Key k1, Key k2) {
        int result = 0;
        Key::iterator i1, i2;
        for (i1 = k1.begin(), i2 = k2.begin(); i1 != k1.end() && i2 != k2.end(); i1++, i2++) {
            if (*i1 == *i2) {
                continue;
            } else if (*i1 < *i2) {
                result = -1;
                break;
            } else {
                result = 1;
                break;
            }
        }
        if (result == 0) {
            if (i1 == k1.end() && i2 == k2.end()) {
                return 0;
            } else if (i1 == k1.end()) {
                return -1;
            } else {
                return 1;
            }
        } else
        {
            return result;
        }
    }

    void Key::free() const {
        delete[](this->data());
    }

    void Key::skip(std::istream& stream) {
        if (stream.eof()) {
            return;
        }
        skip_varlen_bytes(stream);
        if (stream.eof()) {
            throw std::runtime_error("unexpected EOF while skipping key");
        }
    }

    Key::Key(std::istream& stream) {
        *this = Key(read_varlen_bytes(stream));
    }

    void Key::serialize(std::ostream& stream) const {
        write_varlen_bytes(stream, *this);
    }

    Value::Value(bool p_deleted, std::span<std::uint8_t> p_value) 
        : deleted(p_deleted), value(p_value) {};

    void Value::free() const {
        if (this->deleted) {
            return;
        } else {
            delete[](this->value.data());
        }
    }

    void Value::skip(std::istream& stream) {
        bool deleted;
        if (stream.eof()) {
            return;
        }
        stream.read(reinterpret_cast<char*>(&deleted), sizeof(deleted));
        if (stream.eof()) {
            throw std::runtime_error("unexpected EOF while reading deleted");
        }
        skip_varlen_bytes(stream);
    }

    Value::Value(std::istream& stream) {
        if (stream.eof()) {
            // harmless, noop if stream is already eof
            return;
        }
        stream.read(reinterpret_cast<char*>(&this->deleted), sizeof(this->deleted));
        if (stream.eof()) {
            throw std::runtime_error("unexpected EOF while reading deleted for value");
        }
        if (deleted) {
            return;
        } else {
            this->value = read_varlen_bytes(stream);
        }
    }

    void Value::serialize(std::ostream& stream) const {
        stream.write(reinterpret_cast<const char*>(&this->deleted), sizeof(this->deleted));
        if (!this->deleted) {
            write_varlen_bytes(stream, this->value);
        }
    }

    std::span<uint8_t> read_varlen_bytes(std::istream& stream) {
        if (stream.eof()) {
            return std::span<uint8_t>();
        }
        uint32_t n_bytes;
        stream.read(reinterpret_cast<char*>(&n_bytes), sizeof(n_bytes));
        if (stream.eof()) {
            throw std::runtime_error("unexpected EOF while reading length for varlen bytes");
        }
        
        uint8_t* data_buf = new uint8_t[n_bytes];
        stream.read(reinterpret_cast<char*>(data_buf), n_bytes);
        if (stream.eof()) {
            delete[](data_buf);
            throw std::runtime_error("unexpected EOF while reading varlen bytes");
        }
        return std::span(data_buf, n_bytes);
    }

    void write_varlen_bytes(std::ostream &stream, std::span<uint8_t> bytes) {
        uint32_t n_bytes = static_cast<uint32_t>(bytes.size());
        stream.write(reinterpret_cast<char*>(&n_bytes), sizeof(n_bytes));
        stream.write(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    void skip_varlen_bytes(std::istream &stream) {
        if (stream.eof()) {
            return;
        }
        uint32_t n_bytes;
        stream.read(reinterpret_cast<char*>(&n_bytes), sizeof(n_bytes));
        if (stream.eof()) {
            throw std::runtime_error("unexpected EOF while reading bytes length");
        }
        stream.seekg(n_bytes, std::ios_base::cur);
    }

    KVPair KVPair::deserialize(std::istream& stream) {
        Key key(stream);
        if (stream.eof()) {
            throw std::runtime_error("unexpected EOF parsing KVPair");
        }
        Value val(stream);
        if (stream.eof()) {
            throw std::runtime_error("unexpected EOF parsing KVPair");
        }
        return KVPair(key, val);
    }
}