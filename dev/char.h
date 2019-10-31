#pragma once

struct dev_char {
    struct dev_file_ops* ops;
    int internal_id;
};

int  dev_register_char(char* name, struct dev_char* dev_char);
void dev_unregister_char(char* name);