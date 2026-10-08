#pragma once

#include <optional>
#include <memory>

namespace keel {
    template<typename T>
    class Cursor {
    public:
        Cursor() = default;
        virtual std::optional<const T> getNext() = 0;
        virtual bool hasNext() = 0;
        virtual ~Cursor() = default;
    };
}