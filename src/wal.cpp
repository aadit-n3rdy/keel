
#include "wal.hpp"

#include <fstream>

namespace keel {
    Wal::Wal(std::string_view dir) {
        std::fstream f(dir.data());
    }
}