
int fs_get_inode_internal(char* path, inode_t* inode) {

    struct filesystem_mount* mount;
    fs_get_mount(path, &mount);

    //printf("boi is '%s'\n", mount->mount_path);

    inode_t* inode_p = kmalloc(sizeof(inode_t));
    
    memset(inode_p, 0, sizeof(inode_t));
    memset(inode, 0, sizeof(inode_t));

    inode_p->id        = 0;
    inode_p->parent_id = 0;
    inode_p->mount     = mount;

    inode->id    = 1;
    inode->mount = mount;

    mount->get_inode(1, inode_p, inode);

    int jumps = 0;
    int i = 1;
    int j = 0;
    int amnt_d = 0;
    int amnt_c = 0;

    int guard = strlen(path);
    char* piece = kmalloc(256);

    char c;

    for (int k = 1; k < guard - 1; k++) {
        c = path[k];

        if (c == '/')
            amnt_d++;
    }

    while (i <= guard) {
        c = path[i];

        if (c == '/' || i == guard) {
            piece[j] = '\0';

            ++i;

            //printf("N: %s\n", piece);

            int count = inode->mount->countd(inode);
            //printf("C: %i\n", count);

            dentry_t* dentries = kmalloc(sizeof(dentry_t) * count);
            inode->mount->listd(inode, dentries, count);

            for (int k = 0; k < count; k++) {

                if (!strcmp(dentries[k].name, piece)) {

                    if (amnt_c != amnt_d)
                        if (dentries[k].type != FS_RECORD_TYPE_DIR) { 
                            printf("%s: Not found\n", path);
                            return FS_ERR_NOT_FOUND;
                        }

                    amnt_c++;

                    uint64_t next_inode_id = dentries[k].inode_id;

                    //printf("NXTINOD: %i\n", next_inode_id);

                    memcpy(inode_p, inode, sizeof(inode_t));

                    inode->id    = next_inode_id;
                    inode->mount = mount;

                    inode->type  = dentries[k].type;
                    
                    int ret = mount->get_inode(next_inode_id, inode_p, inode);
                    if (ret < 0) {
                        printf("%s (%s): Sum other error\n", path, piece);
                        return ret;
                    }
                }
            }

            j = 0;
            continue;
        }

        piece[j++] = c;
        ++i;
    }

    kfree(piece);
    kfree(inode_p);

    return jumps;
}