#ifndef ELI_EXTRA_FS_DIR_H__
#define ELI_EXTRA_FS_DIR_H__

#include "lua.h"

int eli_mkdir(lua_State *L);
int eli_rmdir(lua_State *L);
int eli_read_dir(lua_State *L);
int eli_open_dir(lua_State *L);
int eli_iter_dir(lua_State *L);

int dir_entry_type(lua_State *L);
int dir_create_meta(lua_State *L);
int direntry_create_meta(lua_State *L);

#endif /* ELI_EXTRA_FS_DIR_H__ */