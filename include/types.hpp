#pragma once

#include <span>
#include <cstdint>
#include <iostream>

namespace keel {

    class Key: public std::span<std::uint8_t> {
    public:
        using std::span<uint8_t>::span;
        inline Key dupe() const& {
            uint8_t *keyData = new uint8_t[this->size()];
            std::copy(this->begin(), this->end(), keyData);
            return Key(keyData, this->size());
        }
        inline Key(const std::span<uint8_t>& src) {
            *this = Key(src.data(), src.size());
        }
        Key(std::istream& stream);
        void free();
        void serialize(std::ostream& stream);
    };

    int compare_keys(Key k1, Key k2);

    class Value {
    public:
        Value(bool p_deleted, std::span<std::uint8_t> p_value);
        bool deleted;
        std::span<std::uint8_t> value;
        inline Value dupe() const& {
            uint8_t* valData = new uint8_t[this->value.size()];
            std::copy(this->value.begin(), this->value.end(), valData);
            return Value(
                deleted, 
                std::span<uint8_t>(valData, this->value.size())
            );
        }
        void free();

        Value(std::istream& stream);
        void serialize(std::ostream& stream);
    };

    std::span<uint8_t> read_varlen_bytes(std::istream& stream);
    void write_varlen_bytes(std::ostream& stream, std::span<uint8_t> bytes);
}