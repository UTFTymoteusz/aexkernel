#include "aex/kmem.h"
#include "aex/rcode.h"

#include "dev/dev.h"
#include "dev/block.h"

#include "fs/fs.h"
#include "fs/inode.h"

#include <stdio.h>
#include <string.h>

#include "iso9660types.h"

#include "iso9660.h"

const char cookie[5] = "CD001";

int iso9660_mount_dev(struct filesystem_mount* mount);

struct iso9660private {
    struct iso9660_primary_volume_desc* pvd;
    uint32_t root_lba;
};
typedef struct iso9660private iso9660private_t;

struct filesystem iso9660_filesystem = {
    .name = "iso9660",
    .mount = iso9660_mount_dev,
};

void iso9660_init() {
    fs_register(&iso9660_filesystem);
}

int iso9660_count_dentries(struct filesystem_mount* mount, uint32_t lba, uint32_t length) {
    length = ((length + 2047) / 2048) * 2048;

    int   ret  = 0;
    void* yeet = kmalloc(length);
    void* ptr  = yeet;
    uint32_t read_so_far = 0;

    struct iso9660_dentry* dentry;
    char buffer[256];

    dev_block_dread(mount->dev_id, lba, length / 2048, yeet);

    while (true) {
        dentry = ptr;

        if (read_so_far >= length)
            break;

        if (dentry->len == 0) {
            uint32_t approx = ((read_so_far + 2047) / 2048) * 2048;

            if (approx < length) {
                ptr = yeet + approx;
                read_so_far = approx;

                continue;
            }
            break;
        }
        ++ret;

        ptr         += dentry->len;
        read_so_far += dentry->len;

        memcpy(buffer, &(dentry->filename), dentry->filename_len);
        buffer[dentry->filename_len] = '\0';

        if (read_so_far >= length)
            break;
    }
    kfree(yeet);
    return ret;
}

int iso9660_list_dentries(struct filesystem_mount* mount, uint32_t lba, uint32_t length, dentry_t* dentries, int max) {
    length = ((length + 2047) / 2048) * 2048;

    struct iso9660private* private_data = mount->private_data;

    void* yeet = kmalloc(length);
    void* ptr  = yeet;
    uint32_t read_so_far = 0;
    int ret = 0;

    struct iso9660_dentry* dentry;
    uint64_t inode_id;
    char* filename;
    int   filename_len;

    dev_block_dread(mount->dev_id, lba, length / 2048, yeet);

    while (true) {
        dentry = ptr;

        if (read_so_far >= length)
            break;

        if (dentry->len == 0) {
            uint32_t approx = ((read_so_far + 2047) / 2048) * 2048;

            if (approx < length) {
                ptr = yeet + approx;
                read_so_far = approx;

                continue;
            }
            break;
        }
        if (ret >= max)
            break;

        ptr         += dentry->len;
        read_so_far += dentry->len;

        filename     = (char*)&(dentry->filename);
        filename_len = dentry->filename_len;
        inode_id     = dentry->data_lba.le;

        if (filename[0] == '\0') {
            filename = ".";
            filename_len = 1;

            if (inode_id == private_data->root_lba)
                inode_id = 1;
        }
        else if (filename[0] == '\1') {
            filename = "..";
            filename_len = 2;

            if (inode_id == private_data->root_lba)
                inode_id = 1;
        }
        else if (filename[filename_len - 2] == ';' && filename[filename_len - 1] == '1')
            filename_len -= 2;

        memcpy(dentries[ret].name, filename, filename_len);

        dentries[ret].name[filename_len] = '\0';
        dentries[ret].type = (dentry->flags & 0x02) ? FS_TYPE_DIR : FS_TYPE_FILE;
        dentries[ret].inode_id = inode_id;
        dentries[ret].size   = dentry->data_len.le;
        dentries[ret].blocks = (dentry->data_len.le + (2048 - 1)) % 2048;

        ++ret;

        if (read_so_far >= length)
            break;
    }
    kfree(yeet);
    return ret;
}

int iso9660_countd(inode_t* inode) {
    return iso9660_count_dentries(inode->mount, inode->first_block, inode->size);
}

int iso9660_listd(inode_t* inode, dentry_t* dentries, int max) {
    return iso9660_list_dentries(inode->mount, inode->first_block, inode->size, dentries, max);
}

int iso9660_readb(inode_t* inode, uint64_t lblock, uint16_t count, uint8_t* buffer) {
    struct filesystem_mount* mount = inode->mount;
    uint64_t begin = inode->first_block + lblock;

    return dev_block_dread(mount->dev_id, begin, count, buffer);
}

uint64_t iso9660_getb(inode_t* inode, uint64_t lblock) {
    return inode->first_block + lblock;
}

int iso9660_get_inode(uint64_t id, inode_t* parent, inode_t* inode_target) {
    if (id == 1) {
        iso9660_dentry_t root = ((iso9660private_t*)parent->mount->private_data)->pvd->root;

        inode_target->first_block = root.data_lba.le;
        inode_target->blocks      = (root.data_len.le + (2048 - 1)) / 2048;
        inode_target->size        = root.data_len.le;
        inode_target->parent_id   = 0;
        inode_target->type = FS_TYPE_DIR;

        return 0;
    }
    inode_target->parent_id = parent->id;

    int count = iso9660_countd(parent);
    dentry_t* dentries = kmalloc(sizeof(dentry_t) * count);
    iso9660_listd(parent, dentries, count);

    uint64_t tgt_id = inode_target->id;

    for (int i = 0; i < count; i++)
        if (dentries[i].inode_id == tgt_id) {
            inode_target->first_block = tgt_id;
            inode_target->size   = dentries[i].size;
            inode_target->blocks = dentries[i].blocks;

            kfree(dentries);
            return 0;
        }

    kfree(dentries);
    return FS_ERR_NOT_FOUND;
}

int iso9660_mount_dev(struct filesystem_mount* mount) {
    mount->get_inode = iso9660_get_inode;
    mount->countd    = iso9660_countd;
    mount->listd     = iso9660_listd;
    mount->readb     = iso9660_readb;
    mount->getb      = iso9660_getb;

    mount->flags |= FS_READONLY;

    struct iso9660private* private_data = kmalloc(sizeof(struct iso9660private));
    mount->private_data = private_data;

    int offset    = 0;
    void* yeet    = kmalloc(2048);
    bool complete = false;
    struct iso9660_primary_volume_desc* pvd = kmalloc(sizeof(struct iso9660_primary_volume_desc));

    memset(pvd, 0, sizeof(struct iso9660_primary_volume_desc));

    while (true) {
        if (offset > 24) {
            printf("iso9660: Too many volume descriptors\n");

            kfree(pvd);
            return ERR_GENERAL;
        }
        dev_block_read(mount->dev_id, 64 + (offset++ * 4), 4, yeet);

        struct iso9660_vdesc* a = yeet;

        if (a->type == ISO9660_PRIMARY_VOLUME) {
            complete = true;
            memcpy(pvd, a, sizeof(struct iso9660_primary_volume_desc));
        }
        if (a->type == ISO9660_TERMINATOR)
            break;

        if (memcmp(a->identifier, cookie, 5))
            continue;
    }
    if (!complete) {
        kfree(pvd);
        printf("Failed to mount as iso9660: Primary Volume Descriptor not found\n");

        return ERR_GENERAL;
    }
    private_data->pvd = pvd;
    private_data->root_lba = pvd->root.data_lba.le;

    dev_block_t* block_dev = dev_block_get_data(mount->dev_id);

    mount->block_size   = 2048;
    mount->block_amount = block_dev->total_sectors;

    dev_block_dread(mount->dev_id, pvd->root.data_lba.le, 1, yeet);
    return 0;
}