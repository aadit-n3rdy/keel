#pragma once

#include <span>
#include <cstdint>

namespace keel {
    typedef std::span<std::uint8_t> Key;

    struct Value {
        bool deleted;
        std::span<std::uint8_t> value;
    };
}