#include "lua.h"
#include "lauxlib.h"
#include <errno.h>
#include "lutil.h"

#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32

#include <io.h>

#define LMODE_T int

#define _lchmod _chmod

#else // unix

#include <unistd.h>

#define LMODE_T mode_t

#define _lchmod chmod
#endif

int eli_chmod(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    LMODE_T mode = luaL_checknumber(L, 2);
    return push_result(L, _lchmod(path, mode), NULL);
}

int eli_chown(lua_State *L)
{
#ifdef _WIN32
    errno = ENOSYS; /* = "Function not implemented" */
    return push_result(L, -1, "chown is not supported on Windows");
#else
    const char *path = luaL_checkstring(L, 1);
    uid_t user = luaL_checknumber(L, 2);
    gid_t group = luaL_optnumber(L, 3, -1);
    return push_result(L, chown(path, user, group), NULL);
#endif
}
