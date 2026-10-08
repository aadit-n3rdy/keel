#pragma once

#include "cursor.hpp"
#include <istream>
#include <memory>
#include <span>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

namespace keel {
    void verify_fname_exists(const std::string& fname, bool should_exist);

    typedef std::shared_ptr<const std::vector<uint8_t>> _shared_immutable_slice;

    class Key {
        _shared_immutable_slice data;
    public:
        inline Key(const _shared_immutable_slice& source): data(source) {};
        void serialize(std::ostream& stream) const;
        static Key deserialize(std::istream& stream);
        static void skip(std::istream& stream);
        friend int compare_keys(Key k1, Key k2);
    };

    // class Key: public std::span<std::uint8_t> {
    // public:
    //     using std::span<uint8_t>::span;
    //     inline Key dupe() const& {
    //         uint8_t *keyData = new uint8_t[this->size()];
    //         std::copy(this->begin(), this->end(), keyData);
    //         return Key(keyData, this->size());
    //     }
    //     inline Key(const std::span<uint8_t>& src) {
    //         *this = Key(src.data(), src.size());
    //     }
    //     Key(std::istream& stream);
    //     void free() const;
    //     void serialize(std::ostream& stream) const;
    //     static void skip(std::istream& stream);
    // };

    int compare_keys(Key k1, Key k2);

    class Value {
    public:
        Value(bool p_deleted, _shared_immutable_slice p_value);
        bool deleted;
        _shared_immutable_slice value;

        static void skip(std::istream& stream);

        static Value deserialize(std::istream& stream);
        void serialize(std::ostream& stream) const;
    };

    class KVPair {
    private:
        Key key;
        Value value;
    public:
        inline KVPair(Key p_key, Value p_value): key(p_key), value(p_value){};
        ~KVPair();
        inline Key getKey() const {
            return this->key;
        }
        inline Value getValue() const {
            return this->value;
        }
        inline void serialize(std::ostream& stream) const {
            this->getKey().serialize(stream);
            this->getValue().serialize(stream);
        }
        static KVPair deserialize(std::istream& stream);
    };

    std::vector<uint8_t>* read_varlen_bytes(std::istream& stream);
    void write_varlen_bytes(std::ostream& stream, std::span<uint8_t> bytes);
    void skip_varlen_bytes(std::istream& stream);
}