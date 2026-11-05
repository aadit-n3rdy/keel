#pragma once

#include "cursor.hpp"
#include <istream>
#include <span>
#include <cstdint>
#include <iostream>
#include <utility>

namespace keel {
    void verify_fname_exists(const std::string& fname, bool should_exist);

    class Key: public std::span<const uint8_t> {
    public:
        using std::span<const uint8_t>::span;
        inline Key dupe() const& {
            uint8_t *keyData = new uint8_t[this->size()];
            std::copy(this->begin(), this->end(), keyData);
            return Key(keyData, this->size());
        }
        inline Key(const std::span<const uint8_t>& src) {
            *this = Key(src.data(), src.size());
        }
        Key(std::istream& stream);
        void free() const;
        uint64_t serialize(std::ostream& stream) const;
        uint64_t serialize_requies() const;
        static void skip(std::istream& stream);
    };

    int compare_keys(Key k1, Key k2);

    class Value {
    public:
        Value(bool p_deleted, std::span<const std::uint8_t> p_value);
        bool deleted;
        std::span<const std::uint8_t> value;
        inline Value dupe() const& {
            uint8_t* valData = new uint8_t[this->value.size()];
            std::copy(this->value.begin(), this->value.end(), valData);
            return Value(
                deleted, 
                std::span<uint8_t>(valData, this->value.size())
            );
        }
        void free() const;
        
        static void skip(std::istream& stream);

        Value(std::istream& stream);
        uint64_t serialize(std::ostream& stream) const;
        uint64_t serialize_requies() const;
    };

    class KVPair: public std::pair<Key, Value> {
    public:
        using std::pair<Key, Value>::pair;
        ~KVPair() = default;
        inline Key getKey() const {
            return this->first;
        }
        inline Value getValue() const {
            return this->second;
        }
        inline void free() const {
            this->getKey().free();
            this->getValue().free();
        }
    };

    std::span<const uint8_t> read_varlen_bytes(std::istream& stream);
    uint64_t write_varlen_bytes(std::ostream& stream, std::span<const uint8_t> bytes);
    void skip_varlen_bytes(std::istream& stream);
}