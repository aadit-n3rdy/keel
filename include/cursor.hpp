#pragma once

#include <optional>
#include <memory>

namespace keel {
    template<typename T>
    class Cursor {
    public:
        Cursor();
        virtual std::optional<const T> getNext();
        virtual bool hasNext();
        virtual ~Cursor();
    };
}