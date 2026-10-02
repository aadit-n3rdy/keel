#pragma once

#include "types.hpp"

namespace keel {
    using namespace keel;
    class Memtable
    {
        Memtable();
        virtual keel::Value getOne(Key key);
        virtual ~Memtable();
    };
}