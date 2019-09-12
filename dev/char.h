#pragma once

#include "char.c"

int dev_register_char(char* name, struct dev_char* dev_char);
void dev_unregister_char(char* name);