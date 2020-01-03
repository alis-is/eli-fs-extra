#include "lua.h"
#include "lauxlib.h"

#include "lfile.h"
#include "llocking.h"
#include "ldir.h"
#include "llink.h"
#include "lperm.h"
#include "lpipe.h"

static const struct luaL_Reg eliFsExtra[] = {
    {"file_info", eli_file_info},
    {"file_type", eli_file_type},
    {"open_dir", eli_open_dir},
    {"read_dir", eli_read_dir},
    {"iter_dir", eli_iter_dir},
    {"link", eli_mklink},
    {"mkdir", eli_mkdir},
    {"rmdir", eli_rmdir},
    {"link_info", eli_link_info},
    {"set_file_mode", eli_set_file_mode},
    {"utime", eli_file_utime},
    {"touch", eli_file_utime},
    {"lock_file", eli_file_lock},
    {"unlock_file", eli_file_unlock},
    {"chmod", eli_chmod},
    {"chown", eli_chown},
    {"lock_dir", eli_lock_dir},
    {"unlock_dir", eli_unlock_dir},
    {"pipe", eli_pipe},
    {NULL, NULL},
};

int luaopen_eli_fs_extra(lua_State *L)
{
    dir_create_meta(L);
    direntry_create_meta(L);
    lock_create_meta(L);
    lua_newtable(L);
    luaL_setfuncs(L, eliFsExtra, 0);
    return 1;
}
