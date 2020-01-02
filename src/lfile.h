#include "lua.h"

int eli_file_utime(lua_State *L);
int eli_file_info(lua_State *L);
int eli_link_info(lua_State *L);
int eli_set_file_mode(lua_State *L);
int eli_file_type(lua_State *L);
int _file_type(char *path, char **res);
