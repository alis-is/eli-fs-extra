#include "lua.h"

int lfile_lock(lua_State *L);
int lfile_unlock(lua_State *L);
int eli_lock_dir(lua_State *L);
int eli_unlock_dir(lua_State *L);
int lock_create_meta(lua_State *L);
