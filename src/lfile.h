#include "lua.h"

int lfile_utime(lua_State *L);
int lfile_info(lua_State *L);
int llink_info(lua_State *L);
int lsetmode(lua_State *L);
int lfile_type(lua_State *L);
int _file_type(char *path, char **res);
