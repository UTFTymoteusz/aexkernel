#include "aex/klist.h"
#include "aex/kmem.h"
#include "aex/rcode.h"

#include "dev/cpu.h"
#include "dev/dev.h"
#include "dev/name.h"

#include "kernel/sys.h"

#include <stdio.h>
#include <string.h>

#include "fs.h"
#include "file.h"

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
    if (ret < 0)
        return ret;

    if (inode->type == FS_RECORD_TYPE_DIR) {
        fs_retire_inode(inode);
        return FS_ERR_IS_DIR;
    }
    memset(file, 0, sizeof(file_t));
    file->inode = inode;

    switch (inode->type) {
        case FS_RECORD_TYPE_DEV:
            ret = dev_open(inode->dev_id);
            if (ret < 0)
                return ret;

            break;
    }
    //printf("File size: %i\n", inode->size);
    return 0;
}

// Replace read and write switches with function pointers
int fs_read_internal(inode_t* inode, uint64_t sblock, int64_t len, uint64_t soffset, uint8_t* buffer) {
    struct filesystem_mount* mount = inode->mount;
    uint32_t block_size = mount->block_size;
    uint32_t max_c      = (65536 / block_size) - 1;

    int ret;

    if (soffset != 0) {
        void* tbuffer = kmalloc(block_size);

        ret = mount->readb(inode, sblock, 1, tbuffer);
        if (ret < 0)
            return ret;

        sblock++;
        memcpy(buffer, tbuffer + soffset, fs_clamp(len, block_size - soffset));

        buffer += (block_size - soffset);
        len    -= (block_size - soffset);
        
        kfree(tbuffer);
    }
    if (len <= 0)
        return 0;

    uint64_t curr_b, last_b, cblock;
    uint16_t amnt  = 0;
    int64_t  lenp = len;

    last_b = mount->getb(inode, sblock) - 1;
    cblock = sblock;

    while (true) {
        curr_b = mount->getb(inode, sblock++);
        len   -= block_size;
        amnt++;

        if (((last_b + 1) != curr_b) || (len <= 0) || (amnt >= max_c)) {
            if (lenp < block_size) {
                void* tbuffer = kmalloc(block_size);

                ret = mount->readb(inode, cblock, amnt, tbuffer);
                if (ret < 0)
                    return ret;

                memcpy(buffer, tbuffer, lenp);
                kfree(tbuffer);
            }
            else
                ret = mount->readb(inode, cblock, amnt, buffer);

            if (ret < 0)
                return ret;

            if (len <= 0)
                return 0;

            buffer += amnt * block_size;

            amnt   = 0;
            cblock = sblock;
        }
        lenp -= block_size;
        last_b = curr_b;
    }
    if (len <= 0)
        return 0;

    void* tbuffer = kmalloc(block_size);

    ret = mount->readb(inode, sblock, 1, tbuffer);
    if (ret < 0)
        return ret;

    memcpy(buffer, tbuffer, fs_clamp(len, block_size));
    kfree(tbuffer);

    return 0;
}

int fs_read(file_t* file, uint8_t* buffer, int len) {
    if (len == 0)
        return 0;

    inode_t* inode = file->inode;

    uint64_t size = inode->size;
    uint64_t lent = len;

    switch (inode->type) {
        case FS_RECORD_TYPE_DEV:
            return dev_read(inode->dev_id, buffer, len);
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
        case FS_RECORD_TYPE_FILE:
            fs_read_internal(inode, starting_block, lent, start_offset, buffer);
            break;
        default:
            kpanic("Invalid record type");
            break;
    }
    file->position = dst;
    return lent;
}

int fs_write(file_t* file, uint8_t* buffer, int len) {
    if (len == 0)
        return 0;

    inode_t* inode = file->inode;

    uint64_t lent = len;

    switch (inode->type) {
        case FS_RECORD_TYPE_FILE:
            kpanic("File writing is not yet implemented");
            break;
        case FS_RECORD_TYPE_DEV:
            return dev_write(inode->dev_id, buffer, len);
            break;
        default:
            kpanic("Invalid record type");
            break;
    }
    return lent;
}

int fs_seek(file_t* file, uint64_t pos) {
    file->position = pos;
    return 0;
}

void fs_close(file_t* file) {
    inode_t* inode = file->inode;

    switch (inode->type) {
        case FS_RECORD_TYPE_DEV:
            dev_close(inode->dev_id);
            break;
    }
    fs_retire_inode(inode);
}

long fs_ioctl(file_t* file, long code, void* mem) {
    inode_t* inode = file->inode;

    switch (inode->type) {
        case FS_RECORD_TYPE_DEV:
            return dev_ioctl(inode->dev_id, code, mem);
        default:
            return FS_ERR_NOT_A_DEV;
    }
}