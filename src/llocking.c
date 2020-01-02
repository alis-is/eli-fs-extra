#include "lua.h"
#include "lauxlib.h"
#include "lutil.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32

#include <sys/locking.h>

#else // unix

#include <unistd.h>
#include <fcntl.h>

#endif

#define LOCK_METATABLE "lock metatable"

static int _file_lock(lua_State *L, FILE *fh, const char *mode, const long start, long len, const char *funcname)
{
    int code;
#ifdef _WIN32
    /* lkmode valid values are:
           LK_LOCK    Locks the specified bytes. If the bytes cannot be locked, the program immediately tries again after 1 second. If, after 10 attempts, the bytes cannot be locked, the constant returns an error.
           LK_NBLCK   Locks the specified bytes. If the bytes cannot be locked, the constant returns an error.
           LK_NBRLCK  Same as _LK_NBLCK.
           LK_RLCK    Same as _LK_LOCK.
           LK_UNLCK   Unlocks the specified bytes, which must have been previously locked.

           Regions should be locked only briefly and should be unlocked before closing a file or exiting the program.

           http://msdn.microsoft.com/library/default.asp?url=/library/en-us/vclib/html/_crt__locking.asp
        */
    int lkmode;

    switch (*mode)
    {
    case 'r':
    case 'w':
        lkmode = LK_NBLCK;
        break;
    case 'u':
        lkmode = LK_UNLCK;
        break;
    default:
        return luaL_error(L, "%s: invalid mode", funcname);
    }
    if (!len)
    {
        fseek(fh, 0L, SEEK_END);
        len = ftell(fh);
    }
    fseek(fh, start, SEEK_SET);

    code = _locking(fileno(fh), lkmode, len);
#else
    struct flock f;
    switch (*mode)
    {
    case 'w':
        f.l_type = F_WRLCK;
        break;
    case 'r':
        f.l_type = F_RDLCK;
        break;
    case 'u':
        f.l_type = F_UNLCK;
        break;
    default:
        return luaL_error(L, "%s: invalid mode", funcname);
    }
    f.l_whence = SEEK_SET;
    f.l_start = (off_t)start;
    f.l_len = (off_t)len;
    code = fcntl(fileno(fh), F_SETLK, &f);
#endif
    return (code != -1);
}

/*
** Locks a file.
** @param #1 File handle.
** @param #2 String with lock mode ('w'rite, 'r'ead).
** @param #3 Number with start position (optional).
** @param #4 Number with length (optional).
*/
int eli_file_lock(lua_State *L)
{
    FILE *fh = check_file(L, 1, "lock");
    const char *mode = luaL_checkstring(L, 2);
    const long start = (long)luaL_optinteger(L, 3, 0);
    long len = (long)luaL_optinteger(L, 4, 0);
    if (_file_lock(L, fh, mode, start, len, "lock"))
    {
        lua_pushboolean(L, 1);
        return 1;
    }
    else
    {
        lua_pushnil(L);
        lua_pushfstring(L, "%s", strerror(errno));
        return 2;
    }
}

/*
** Unlocks a file.
** @param #1 File handle.
** @param #2 Number with start position (optional).
** @param #3 Number with length (optional).
*/
int eli_file_unlock(lua_State *L)
{
    FILE *f = check_file(L, 1, "unlock");
    const long start = (long)luaL_optinteger(L, 2, 0);
    long len = (long)luaL_optinteger(L, 3, 0);
    if (_file_lock(L, f, "u", start, len, "unlock"))
    {
        lua_pushboolean(L, 1);
        return 1;
    }
    else
    {
        lua_pushnil(L);
        lua_pushfstring(L, "%s", strerror(errno));
        return 2;
    }
}

#ifdef _WIN32
typedef struct lfs_Lock
{
    HANDLE fd;
} lfs_Lock;
int eli_lock_dir(lua_State *L)
{
    size_t pathl;
    HANDLE fd;
    lfs_Lock *lock;
    char *ln;
    const char *lockfile = "/lockfile";
    const char *path = luaL_checklstring(L, 1, &pathl);
    ln = joinpath(path, lockfile);
    if (!ln)
    {
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        return 2;
    }
    if ((fd = CreateFile(ln, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL)) == INVALID_HANDLE_VALUE)
    {
        int en = GetLastError();
        free(ln);
        lua_pushnil(L);
        if (en == ERROR_FILE_EXISTS || en == ERROR_SHARING_VIOLATION)
            lua_pushstring(L, "File exists");
        else
            lua_pushstring(L, strerror(en));
        return 2;
    }
    free(ln);
    lock = (lfs_Lock *)lua_newuserdata(L, sizeof(lfs_Lock));
    lock->fd = fd;
    luaL_getmetatable(L, LOCK_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}
int eli_unlock_dir(lua_State *L)
{
    lfs_Lock *lock = (lfs_Lock *)luaL_checkudata(L, 1, LOCK_METATABLE);
    if (lock->fd != INVALID_HANDLE_VALUE)
    {
        CloseHandle(lock->fd);
        lock->fd = INVALID_HANDLE_VALUE;
    }
    return 0;
}
#else
typedef struct lfs_Lock
{
    char *ln;
} lfs_Lock;

int eli_lock_dir(lua_State *L)
{
    lfs_Lock *lock;
    size_t pathl;
    char *ln;
    const char *lockfile = "/lockfile";
    const char *path = luaL_checklstring(L, 1, &pathl);
    ln = joinpath(path, lockfile);
    if (!ln)
    {
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        return 2;
    }
    if (symlink("lock", ln) == -1)
    {
        free(ln);
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        return 2;
    }
    lock = (lfs_Lock *)lua_newuserdata(L, sizeof(lfs_Lock));
    lock->ln = ln;
    
    luaL_getmetatable(L, LOCK_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}

int eli_unlock_dir(lua_State *L)
{
    lfs_Lock *lock = (lfs_Lock *)luaL_checkudata(L, 1, LOCK_METATABLE);
    if (lock->ln)
    {
        unlink(lock->ln);
        free(lock->ln);
        lock->ln = NULL;
    }
    return 0;
}
#endif

/*
** Creates lock metatable.
*/
int lock_create_meta(lua_State *L)
{
    luaL_newmetatable(L, LOCK_METATABLE);

    /* Method table */
    lua_newtable(L);
    lua_pushcfunction(L, eli_unlock_dir);
    lua_setfield(L, -2, "free");
    lua_pushcfunction(L, eli_unlock_dir);
    lua_setfield(L, -2, "unlock");

    /* Metamethods */
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, eli_unlock_dir);
    lua_setfield(L, -2, "__gc");
    return 1;
}
