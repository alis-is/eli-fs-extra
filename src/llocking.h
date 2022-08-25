#include "lua.h"

int eli_file_lock(lua_State *L);
int eli_file_unlock(lua_State *L);
int eli_is_lock_active(lua_State *L);

int eli_lock_dir(lua_State *L);
int eli_unlock_dir(lua_State *L);
int eli_is_dir_lock_active(lua_State *L);

int lock_create_meta(lua_State *L);
int dir_lock_create_meta(lua_State *L);
