
#include "wal.hpp"
#include "types.hpp"

#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <ios>
#include <ostream>
#include <stdexcept>
#include <string>

namespace keel {
    static void wal_fname_append(std::string &dir, int id) {
        dir.append("/wal_");
        dir.append(std::to_string(id));
        dir.append(".keelwal");
    }

    std::string verified_wal_fname(const char* dir, int id, bool should_exist) {
        if (!std::filesystem::is_directory(dir)) {
            throw std::runtime_error("invalid directory");
        }

        std::string fname = dir;

        wal_fname_append(fname, id);

        bool exists = std::filesystem::exists(fname);
        if (!should_exist && exists) {
            throw std::runtime_error("WAL file already exists"); 
        } else if (should_exist && (!exists || !std::filesystem::is_regular_file(fname))) {
            throw std::runtime_error("WAL file does not exist"); 
        }
        return fname;
    }

    Wal::Wal(const char* dir, int id) : file(
        verified_wal_fname(dir, id, false),
        std::ios_base::binary
    ) {
        this->file.exceptions(std::ios::badbit | std::ios::failbit);
    };

    Wal::~Wal() {
        if (this->file.is_open()) {
            this->file.close();
        }
    }

    void Wal::Operation::serialize(std::ostream& stream) {
        stream.write(reinterpret_cast<char*>(&this->type), sizeof(this->type));
        this->key.serialize(stream); 
        switch (this->type) { 
        case OpType::SET: 
            if (this->value.has_value()) {
                this->value->serialize(stream);
            } else {
                throw std::runtime_error("expected value for non-DELETE op");
            }
            break;
        case OpType::DELETE:
            break;
        default:
            throw std::runtime_error("unknown OpType");
        }
    }

    Wal::Operation::Operation(std::istream& stream) : value(Value(true, std::span<uint8_t>())){
        stream.read(reinterpret_cast<char*>(&this->type), sizeof(this->type));
        if (stream.eof()) {
            return;
        }
        this->key = Key(stream);
        if (stream.eof()) {
            return;
        }
        switch(this->type) {
        case OpType::SET:
            this->value = Value(stream);
            if (stream.eof()) {
                this->key.free();
                return;
            }
            break;
        case OpType::DELETE:
            break;
        default:
            throw std::runtime_error("unknown OpType");
        }
    }

    void Wal::Operation::free() {
        this->key.free();
        if (this->type != OpType::DELETE) {
            if (this->value.has_value()) {
                this->value->free();
            }
        }
    }

    WalReader::WalReader(const char* dir, int id): file(verified_wal_fname(dir, id, true)) {};

    std::optional<Wal::Operation> WalReader::read_next() {
        if (this->file.eof()) {
            return std::optional<Wal::Operation>();
        }
        Wal::Operation op(this->file);
        if (this->file.eof()) {
            return std::optional<Wal::Operation>();
        } else {
            return op;
        }
    }
}