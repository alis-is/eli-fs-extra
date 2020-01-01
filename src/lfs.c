#include "lua.h"
#include "lauxlib.h"

#include "lfile.h"
#include "llocking.h"
#include "ldir.h"
#include "llink.h"
#include "lperm.h"

static const struct luaL_Reg eliFsExtra[] = {
    {"file_info", lfile_info},
    {"file_type", lfile_type},
    {"open_dir", lopendir},
    {"read_dir", lreaddir},
    {"iter_dir", lwalkdir},
    {"link", lmklink},
    {"mkdir", lmkdir},
    {"link_info", llink_info},
    {"set_mode", lsetmode},
    {"utime", lfile_utime},
    {"touch", lfile_utime},
    {"lock_file", lfile_lock},
    {"unlock_file", lfile_unlock},
    {"chmod", eli_chmod},
    {"chown", eli_chown},
    {"lock_dir", eli_lock_dir},
    {"unlock_dir", eli_unlock_dir},
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
