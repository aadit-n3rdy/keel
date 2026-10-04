#include "types.hpp"
#include <cstdint>
#include <ios>
#include <stdexcept>
#include <sys/types.h>

namespace keel {
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

    void Key::free() {
        delete[](this->data());
    }

    Key::Key(std::istream& stream) {
        *this = Key(read_varlen_bytes(stream));
    }

    void Key::serialize(std::ostream& stream) {
        write_varlen_bytes(stream, *this);
    }

    Value::Value(bool p_deleted, std::span<std::uint8_t> p_value) 
        : deleted(p_deleted), value(p_value) {};

    void Value::free() {
        if (this->deleted) {
            return;
        } else {
            delete[](this->value.data());
        }
    }

    Value::Value(std::istream& stream) {
        if (stream.eof()) {
            // harmless, noop if stream is already eof
            return;
        }
        stream >> this->deleted;
        if (stream.eof()) {
            throw std::runtime_error("unexpected EOF while reading deleted for value");
        }
        if (deleted) {
            return;
        } else {
            this->value = read_varlen_bytes(stream);
        }
    }

    void Value::serialize(std::ostream& stream) {
        stream << this->deleted;
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
}