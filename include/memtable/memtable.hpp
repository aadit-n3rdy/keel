#pragma once

#include "types.hpp"

#include <optional>

#include "cursor.hpp"

namespace keel {
    using namespace keel;
    class Memtable
    {
    public:
        Memtable();
        virtual std::optional<const keel::Value> getOne(keel::Key key) = 0;
        virtual void set(keel::Key const key, keel::Value const value) = 0;
        virtual Cursor<const KVPair>* getAll() = 0;
        virtual ~Memtable() = 0;
    };
}