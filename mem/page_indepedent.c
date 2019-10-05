#pragma once

size_t mempg_to_pages(size_t bytes) {
    size_t  amnt = 0;
    int64_t bytes_s = (int64_t)bytes;

    while (bytes_s > 0) {
        ++amnt;
        bytes_s -= CPU_PAGE_SIZE;
    }
    return amnt;
}