#pragma once


#include "types.hpp"

#include <concepts>
#include <cstdio>
#include <iterator>
#include <fstream>
#include <optional>
#include <stdexcept>

namespace keel {
    class Wal {
    private:
        std::ofstream file;
        std::string fname;
    public:
        enum class OpType {
            SET,
            DELETE
        };


        Wal() = delete;
        Wal(Wal&) = delete;
        Wal(Wal*) = delete;

        class Operation {
        private:
            inline Operation(OpType p_type, Key p_key, std::optional<Value> p_value)
                : type(p_type), key(p_key), value(p_value) {};
        public:
            OpType type;
            Key key;
            std::optional<Value> value;
            void serialize(std::ostream& stream);
            Operation(std::istream& stream);
            void free(); // to be called only if serialized
            static inline Operation DELETE(Key p_key) {
                return Operation(
                    OpType::DELETE,
                    p_key,
                    std::optional<Value>()
                );
            }
            static inline Operation SET(Key p_key, Value p_value) {
                return Operation(
                    OpType::SET,
                    p_key,
                    std::optional<Value>(p_value)
                );
            }
        };

        Wal(const char* dir, int id);
        ~Wal();

        inline void close() {
            this->file.close();
        }

        template <std::input_iterator OpIterator>
            requires std::convertible_to<std::iter_value_t<OpIterator>, Operation>
        void append_atomic(OpIterator first, OpIterator last) {
            for(OpIterator cur = first; cur != last; cur++) {
                cur->serialize(this->file);
            }
            this->file.flush();
        }
    };

    class WalReader {
    private:
        std::ifstream file;
    public:
        WalReader(const char* dir, int id);
        std::optional<Wal::Operation> read_next(); 
    };
}