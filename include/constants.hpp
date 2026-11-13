#pragma once

#include <cstdint>

namespace keel {
    const uint64_t SSTABLE_SPINDEX_CHUNK_SIZE = 32;
    const uint64_t SSTABLE_PAGE_SIZE = 4*(1<<12);
    const uint64_t SSTABLE_PAGE_BITS = ~uint64_t(SSTABLE_PAGE_SIZE - 1);
}
