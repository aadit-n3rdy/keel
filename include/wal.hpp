#pragma once


#include "types.hpp"

#include <string_view>

namespace keel {
    class Wal {
        FILE* fptr;
    public:
        enum class OpType {
            SET,
            DELETE
        };

        /// Owns nothing, just a transient view
        class Operation {
        public:
            OpType type;
            Key key;
            Value value;
            void serialize(std::ostream& stream);
        };

        Wal(std::string_view dir);
        ~Wal();
        void append_atomic(const Operation op);
    };

}