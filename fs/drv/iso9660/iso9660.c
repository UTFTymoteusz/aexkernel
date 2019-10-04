#pragma once

#include "fs/fs.h"
#include "fs/inode.h"

#include "iso9660types.h"

const char cookie[5] = "CD001";

int iso9660_mount_dev(struct filesystem_mount* mounted);

struct iso9660private {
    struct iso9660_primary_volume_desc* pvd;
};
typedef struct iso9660private iso9660private_t;

struct filesystem iso9660_filesystem = {
    .name = "iso9660",
    .mount = iso9660_mount_dev,
};

void iso9660_init() {
    fs_register(&iso9660_filesystem);
}

int iso9660_count_dentries(struct filesystem_mount* mounted, uint32_t lba, uint32_t length) {

    length = ((length + 2047) / 2048) * 2048;

    int ret = 0;
    void* yeet = kmalloc(length);
    void* ptr  = yeet;

    char buffer[256];

    uint32_t read_so_far = 0;

    struct iso9660_dentry* dentry;

    /*if (length > 0x2000) {
        printf("LBA: %i\n", lba);
        sleep(1000);
    }*/

    dev_disk_dread(mounted->dev_id, lba, length / 2048, yeet);

    while (true) {

        dentry = ptr;
        
        if (read_so_far >= length)
            break;

        if (dentry->len == 0) {
            uint32_t approx = ((read_so_far + 2047) / 2048) * 2048;

            /*if (length > 0x2000) {
                printf("uhh %i - %i\n", approx, read_so_far);
                printf("%x\n", 0xC000 + approx);
                
                printf("%x\n", ((uint8_t*)ptr)[0]);

                sleep(500);
            }*/

            if (approx < length) {
                ptr = yeet + approx;
                read_so_far = approx;

                continue;
            }

            break;
        }

        ++ret;
        ptr += dentry->len;
        read_so_far += dentry->len;

        memcpy(buffer, &(dentry->filename), dentry->filename_len);
        buffer[dentry->filename_len] = '\0';

        //printf("%i\n", dentry->filename_len);
        //printf("%i. %s\n", ret, buffer);

        if (read_so_far >= length)
            break;
    }

    kfree(yeet);
    return ret;
}
int iso9660_list_dentries(struct filesystem_mount* mounted, uint32_t lba, uint32_t length, dentry_t* dentries, int max) {

    length = ((length + 2047) / 2048) * 2048;

    int ret = 0;
    void* yeet = kmalloc(length);
    void* ptr  = yeet;

    uint32_t read_so_far = 0;

    struct iso9660_dentry* dentry;

    dev_disk_dread(mounted->dev_id, lba, length / 2048, yeet);

    char* filename;
    int filename_len;

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

        ptr += dentry->len;
        read_so_far += dentry->len;

        filename     = (char*)&(dentry->filename);
        filename_len = dentry->filename_len;

        if (filename[0] == '\0') {
            filename = ".";
            filename_len = 1;
        }
        else if (filename[0] == '\1') {
            filename = "..";
            filename_len = 2;
        }
        else if (filename[filename_len - 2] == ';' && filename[filename_len - 1] == '1')
            filename_len -= 2;

        memcpy(dentries[ret].name, filename, filename_len);
        dentries[ret].name[filename_len] = '\0';

        dentries[ret].type = (dentry->flags & 0x02) ? FS_RECORD_TYPE_DIR : FS_RECORD_TYPE_FILE;
        dentries[ret].inode_id = dentry->data_lba.le;

        dentries[ret].size   = dentry->data_len.le;
        dentries[ret].blocks = (dentry->data_len.le + (2048 - 1)) % 2048;

        ++ret;

        //printf("%i\n", dentry->filename_len);
        //printf("%i. %s\n", ret, buffer);

        if (read_so_far >= length)
            break;
    }

    kfree(yeet);
    return ret;
}

int iso9660_countd(inode_t* inode) {
    
    //printf("iso9660: They want to countd inode %i\n", inode->id);

    return iso9660_count_dentries(inode->mount, inode->first_block, inode->size);
}
int iso9660_listd(inode_t* inode, dentry_t* dentries, int max) {
    
    //printf("iso9660: They want to listd inode %i\n", inode->id);

    return iso9660_list_dentries(inode->mount, inode->first_block, inode->size, dentries, max);
}

int iso9660_get_inode(uint64_t id, inode_t* parent, inode_t* inode_target) {

    //printf("iso9660: They want inode %i, which parent is %i\n", id, parent->id);

    if (id == 1) {
        iso9660_dentry_t root = ((iso9660private_t*)parent->mount->private_data)->pvd->root;

        inode_target->first_block = root.data_lba.le;
        inode_target->blocks      = (root.data_len.le + (2048 - 1)) / 2048;
        inode_target->size        = root.data_len.le;
    
        inode_target->parent_id = 0;

        inode_target->type = FS_RECORD_TYPE_DIR;

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

            break;
        }
    
    kfree(dentries);

    return -1;
}

int iso9660_mount_dev(struct filesystem_mount* mounted) {

    mounted->get_inode = iso9660_get_inode;
    mounted->countd    = iso9660_countd;
    mounted->listd     = iso9660_listd;

    struct iso9660private* private_data = mounted->private_data;

    //printf("Attempting to mount iso9660\n");

    void* yeet = kmalloc(2048);

    bool complete = false;

    struct iso9660_primary_volume_desc* pvd = kmalloc(sizeof(struct iso9660_primary_volume_desc));
    memset(pvd, 0, sizeof(struct iso9660_primary_volume_desc));

    int offset = 0;

    while (true) {

        if (offset > 24) {
            printf("iso9660: Too many volume descriptors\n");

            kfree(pvd);
            return -1;
        }

        dev_disk_read(mounted->dev_id, 64 + (offset++ * 4), 4, yeet);
        struct iso9660_vdesc* a = yeet;

        if (a->type == ISO9660_PRIMARY_VOLUME) {
            complete = true;
            memcpy(pvd, a, sizeof(struct iso9660_primary_volume_desc));
        }

        if (a->type == ISO9660_TERMINATOR)
            break;

        if (memcmp(a->identifier, cookie, 5)) { 
            //printf("iso9660: Damaged volume descriptor at index %i\n", offset / 4);
            continue;
        }
    }

    if (!complete) {
        kfree(pvd);
        printf("Failed to mount as iso9660: Primary Volume Descriptor not found\n");
        
        return -1;
    }

    private_data->pvd = pvd;

    dev_disk_dread(mounted->dev_id, pvd->root.data_lba.le, 1, yeet);

    //printf("Len  : %i\n", pvd->root.data_len.le);
    //printf("%x\n", ((uint8_t*)yeet)[0]);

    //printf("Count: %i\n", iso9660_count_dentries(mounted, pvd->root.data_lba.le, pvd->root.data_len.le));
    //printf("%x\n", pvd->unused3);
    return 0;
}