#include "lua.h"

int lmkdir(lua_State *L);
int lreaddir(lua_State *L);
int lopendir(lua_State *L);
int lwalkdir(lua_State *L);
int dir_entry_type(lua_State *L);
int dir_create_meta(lua_State *L);
int direntry_create_meta(lua_State *L);
