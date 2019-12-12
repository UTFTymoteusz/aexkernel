#pragma once

int dev_name2id(char* name);
int dev_id2name(int id, char* buffer);

// This accepts an input like "sd@" and spits out sda, sdb, sdc and so on
char* dev_name_inc(char* pattern, char* buffer);