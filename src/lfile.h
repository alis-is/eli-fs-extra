#ifndef ELI_EXTRA_FS_FILE_H__
#define ELI_EXTRA_FS_FILE_H__

#include "lua.h"

int eli_file_utime(lua_State *L);
int eli_file_info(lua_State *L);
int eli_link_info(lua_State *L);
int eli_set_file_mode(lua_State *L);
int eli_file_type(lua_State *L);
int eli_link_type(lua_State *L);
int _file_type(const char *path, const char **res);

#endif /* ELI_EXTRA_FS_FILE_H__ */