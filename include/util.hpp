#pragma once

#include <cerrno>
#include <stdexcept>
#include <system_error>

namespace keel {
    inline void throw_errno() {
        int errnum = errno;
        std::error_code ec(errnum, std::generic_category());
        printf("ERROR: %s\n", ec.message().c_str());
        throw std::runtime_error("operation failed due to: " + ec.message());
    }
}