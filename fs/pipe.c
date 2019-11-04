#include "aex/kmem.h"
#include "aex/rcode.h"

#include "fs/fs.h"
#include "fs/file.h"

#include <string.h>

#include "pipe.h"

int fs_pipe_create(file_t* reader, file_t* writer, size_t size) {
    pipe_t* pipe = kmalloc(sizeof(pipe_t));
    memset(pipe, 0, sizeof(pipe_t));
    memset(reader, 0, sizeof(file_t));
    memset(writer, 0, sizeof(file_t));

    int ret = cbuf_create(&(pipe->cbuf), size);
    if (ret < 0)
        return ret;

    reader->type = FILE_TYPE_PIPE;
    writer->type = FILE_TYPE_PIPE;

    reader->flags |= FILE_FLAG_READ;
    writer->flags |= FILE_FLAG_WRITE;

    reader->pipe = pipe;
    writer->pipe = pipe;

    return 0;
}

int fs_pipe_read(file_t* file, uint8_t* buffer, int len) {
    if (!(file->flags & FILE_FLAG_READ))
        return FS_ERR_WRONG_MODE;

    cbuf_read(&(file->pipe->cbuf), buffer, len);
    return len;
}

int fs_pipe_write(file_t* file, uint8_t* buffer, int len) {
    if (!(file->flags & FILE_FLAG_WRITE))
        return FS_ERR_WRONG_MODE;

    cbuf_write(&(file->pipe->cbuf), buffer, len);
    return len;
}