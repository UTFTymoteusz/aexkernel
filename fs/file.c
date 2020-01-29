#include "aex/aex.h"
#include "aex/kernel.h"
#include "aex/klist.h"
#include "aex/mem.h"
#include "aex/rcode.h"
#include "aex/string.h"
#include "aex/sys.h"

#include "aex/dev/block.h"
#include "aex/dev/char.h"
#include "aex/dev/name.h"

#include "aex/fs/fs.h"
#include "aex/fs/file.h"

inline int64_t fs_clamp(int64_t val, int64_t max) {
    if (val > max)
        return max;

    if (val < 0)
        return 0;

    return val;
}

int fs_open(char* path, file_t* file) {
    inode_t* inode = NULL;

    int ret = fs_get_inode(path, &inode);
    RETURN_IF_ERROR(ret);

    if (inode->type == FS_TYPE_DIR) {
        fs_retire_inode(inode);
        return ERR_IS_DIR;
    }
    memset(file, 0, sizeof(file_t));
    file->inode = inode;
    file->ref_count++;

    switch (inode->type) {
        case FS_TYPE_CDEV:
            ret = dev_char_open(inode->dev_id);
            RETURN_IF_ERROR(ret);
            break;
    }
    return 0;
}

// TODO: Replace read and write switches with function pointers, maybe
int fs_read_internal(inode_t* inode, uint64_t sblock, int64_t len, uint64_t soffset, uint8_t* buffer) {
    struct filesystem_mount* mount = inode->mount;
    uint32_t block_size = mount->block_size;
    uint32_t max_c      = (65536 / block_size) - 1;

    int ret;

    if (soffset != 0) {
        CLEANUP void* tbuffer = kmalloc(block_size);

        ret = mount->readb(inode, sblock, 1, tbuffer);
        RETURN_IF_ERROR(ret);

        sblock++;
        memcpy(buffer, tbuffer + soffset, fs_clamp(len, block_size - soffset));

        buffer += (block_size - soffset);
        len    -= (block_size - soffset);
    }
    if (len <= 0)
        return 0;

    uint64_t tmp_block, last_block, curr_block;
    uint16_t amnt  = 0;
    int64_t  lenp = len;

    last_block = mount->getb(inode, sblock) - 1;
    curr_block = sblock;

    while (true) {
        tmp_block = mount->getb(inode, sblock++);
        len -= block_size;
        amnt++;

        if ((last_block + 1) != tmp_block || len <= 0 || amnt >= max_c) {
            if (lenp < block_size) {
                if (amnt > 1) {
                    --amnt;

                    ret = mount->readb(inode, curr_block, amnt, buffer);
                    RETURN_IF_ERROR(ret);

                    curr_block += amnt;
                    buffer += (amnt * block_size);

                    amnt = 0;
                }
                void* tbuffer = kmalloc(block_size);

                ret = mount->readb(inode, curr_block, 1, tbuffer);
                RETURN_IF_ERROR(ret);

                memcpy(buffer, tbuffer, lenp);
                kfree(tbuffer);
            }
            else
                ret = mount->readb(inode, curr_block, amnt, buffer);

            RETURN_IF_ERROR(ret);

            if (len <= 0)
                return 0;

            buffer += (amnt * block_size);
            amnt   = 0;
            curr_block = sblock;
        }
        lenp -= block_size;
        last_block = tmp_block;
    }
    if (len <= 0)
        return 0;

    void* tbuffer = kmalloc(block_size);

    ret = mount->readb(inode, sblock, 1, tbuffer);
    RETURN_IF_ERROR(ret);

    memcpy(buffer, tbuffer, fs_clamp(len, block_size));
    kfree(tbuffer);

    return 0;
}

int fs_read(file_t* file, uint8_t* buffer, int len) {
    if (len == 0)
        return 0;

    switch (file->type) {
        case FILE_TYPE_PIPE:
            return fs_pipe_read(file, buffer, len);
        default:
            break;
    }
    inode_t* inode = file->inode;

    uint64_t size = inode->size;
    uint64_t lent = len;

    switch (inode->type) {
        case FS_TYPE_CDEV:
            return dev_char_read(inode->dev_id, buffer, len);
    }
    if (file->position + lent >= size)
        lent = size - file->position;

    if (file->position == size)
        return 0;

    uint32_t block_size = inode->mount->block_size;
    uint64_t dst = file->position + lent;
    uint64_t starting_block = (file->position + 1) / block_size;
    uint32_t start_offset   = file->position - starting_block * block_size;

    switch (inode->type) {
        case FS_TYPE_FILE:
            fs_read_internal(inode, starting_block, lent, start_offset, buffer);
            file->position = dst;
            break;
        default:
            printk("TYPE: %i\n", inode->type);
            kpanic("Invalid record type");
            break;
    }
    return lent;
}

int fs_write(file_t* file, uint8_t* buffer, int len) {
    if (len == 0)
        return 0;

    switch (file->type) {
        case FILE_TYPE_PIPE:
            return fs_pipe_write(file, buffer, len);
        default:
            break;
    }
    inode_t* inode = file->inode;
    uint64_t lent = len;

    switch (inode->type) {
        case FS_TYPE_FILE:
            kpanic("File writing is not yet implemented");
            break;
        case FS_TYPE_CDEV:
            return dev_char_write(inode->dev_id, buffer, len);
            break;
        default:
            printk("TYPE: %i\n", inode->type);
            kpanic("Invalid record type");
            break;
    }
    return lent;
}

void fs_close(file_t* file) {
    inode_t* inode = file->inode;
    file->ref_count--;

    switch (inode->type) {
        case FS_TYPE_CDEV:
            dev_char_close(inode->dev_id);
            break;
    }
    fs_retire_inode(inode);
}

int fs_seek(file_t* file, uint64_t pos) {
    file->position = pos;
    return 0;
}

int _file_ioctl(file_t* file, long code, void* mem) {
    switch (code) {
        case IOCTL_BYTES_AVAILABLE:
            *((int*) mem) = file->inode->size - file->position;
            return 0;
        default:
            return -1;
    }
}

long fs_ioctl(file_t* file, long code, void* mem) {
    inode_t* inode = file->inode;

    switch (inode->type) {
        case FS_TYPE_CDEV:
            return dev_char_ioctl(inode->dev_id, code, mem);
        case FS_TYPE_FILE:
            return _file_ioctl(file, code, mem);
        default:
            return -1;
    }
}