#include <stddef.h>
#include <stdint.h>

struct cbufm {
    size_t write_ptr;
    size_t size;

    uint8_t* buffer;
};
typedef struct cbufm cbufm_t;

int cbufm_create(cbufm_t* cbuf, size_t size);

int cbufm_read(cbufm_t* cbufm, uint8_t* buffer, size_t start, size_t len);
size_t cbufm_write(cbufm_t* cbufm, uint8_t* buffer, size_t len);