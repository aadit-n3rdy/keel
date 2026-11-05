#pragma once

#include "types.hpp"
#include <optional>

namespace keel {
    template<typename T>
    class Cursor {
    public:
        Cursor() = default;
        virtual std::optional<const T> getNext() = 0;
        virtual ~Cursor() = default;
    };

    template<typename T>
    class SkippableCursor: public Cursor<T> {
    public:
        SkippableCursor() = default;
        virtual bool skipNext() = 0; // skips the next element and returns true if there was an element to skip.
        virtual std::optional<T> getLast() = 0; // returns the last value from the cursor, populated only when skipNext has nothing left to skip
        virtual ~SkippableCursor() = default;
    };
}