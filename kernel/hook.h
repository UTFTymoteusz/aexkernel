#pragma once

int _hook_pstart;
#define HOOK_PSTART _hook_pstart
int _hook_pkill;
#define HOOK_PKILL _hook_pkill
int _hook_usr_faccess;
#define HOOK_USR_FACCESS _hook_usr_faccess

#define MAX_HOOKS 64

void hook_add   (int id, char* name, void* func);
void hook_remove(int id, char* name);

void hook_invoke(int id, void* data);

void hook_first(int id, void** pointer);
void* hook_invoke_advance(int id, void* data, void** pointer);