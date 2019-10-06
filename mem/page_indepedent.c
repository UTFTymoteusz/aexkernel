#pragma once

inline size_t mempg_to_pages(size_t bytes) {
    return (bytes + (CPU_PAGE_SIZE - 1)) / CPU_PAGE_SIZE;
}