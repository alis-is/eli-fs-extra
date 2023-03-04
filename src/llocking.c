#include "lua.h"
#include "lauxlib.h"
#include "lutil.h"
#include "lfsutil.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32

#include <sys/locking.h>

#else // unix

#include <unistd.h>
#include <fcntl.h>

#endif

#define LOCK_METATABLE "ELI_FILE_LOCK"
#define LOCK_DIR_METATABLE "ELI_DIR_LOCK"

static int _file_lock(lua_State *L, FILE *fh, const char *mode,
                      const long start, long len, const char *funcname) {
  int code;
#ifdef _WIN32
  /* lkmode valid values are:
         LK_LOCK    Locks the specified bytes. If the bytes cannot be locked,
     the program immediately tries again after 1 second. If, after 10 attempts,
     the bytes cannot be locked, the constant returns an error. LK_NBLCK   Locks
     the specified bytes. If the bytes cannot be locked, the constant returns an
     error. LK_NBRLCK  Same as _LK_NBLCK. LK_RLCK    Same as _LK_LOCK. LK_UNLCK
     Unlocks the specified bytes, which must have been previously locked.

         Regions should be locked only briefly and should be unlocked before
     closing a file or exiting the program.

         http://msdn.microsoft.com/library/default.asp?url=/library/en-us/vclib/html/_crt__locking.asp
      */
  int lkmode;

  switch (*mode) {
  case 'r':
  case 'w':
    lkmode = _LK_NBLCK;
    break;
  case 'u':
    lkmode = _LK_UNLCK;
    break;
  default:
    return luaL_error(L, "%s: invalid mode", funcname);
  }
  if (!len) {
    fseek(fh, 0L, SEEK_END);
    len = ftell(fh);
  }
  fseek(fh, start, SEEK_SET);
  code = _locking(fileno(fh), lkmode, len);
#else
  struct flock f;
  switch (*mode) {
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

typedef struct efs_lock {
  FILE *file;
  int ownsFile;
  long start;
  long len;
} efs_lock;

/*
** Locks a file.
** @param #1 File handle.
** @param #2 String with lock mode ('w'rite, 'r'ead).
** @param #3 Number with start position (optional).
** @param #4 Number with length (optional).
*/
int eli_file_lock(lua_State *L) {
  FILE *fh;
  const int argt = lua_type(L, 1);
  int ownsFile = 0;
  switch (argt) {
  case LUA_TUSERDATA:
    fh = check_file(L, 1, "lock");
    break;
  case LUA_TSTRING:
    const char *path = luaL_checkstring(L, 1);
    fh = fopen(path, "ab");
    if (fh == 0) {
      return push_error(L, "lock");
    }
    ownsFile = 1;
    break;
  default:
    luaL_error(L, "lock: Invalid type (%s) string or FILE* expected.", argt);
    return 0;
  }
  const long start = (long)luaL_optinteger(L, 3, 0);
  const long len = (long)luaL_optinteger(L, 4, 0);
  const char *mode = luaL_checkstring(L, 2);
  
  if (_file_lock(L, fh, mode, start, len, "lock")) {
    efs_lock *lock = (efs_lock *)lua_newuserdata(L, sizeof(efs_lock));
    lock->ownsFile = ownsFile;
    lock->file = fh;
    lock->start = start;
    lock->len = len;
    luaL_getmetatable(L, LOCK_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
  } else {
    if (ownsFile) {
        fclose(fh);
    }
    #if _WIN32
      return windows_pushlasterror(L);
    #else
      return push_error(L, "lock");
    #endif
  }
}

/*
** Unlocks a file.
** @param #1 File handle.
** @param #2 Number with start position (optional).
** @param #3 Number with length (optional).
*/
int eli_file_unlock(lua_State *L) {
  efs_lock *lock = (efs_lock *)luaL_checkudata(L, 1, LOCK_METATABLE);
  if (lock->file != NULL) {
    if (lock->ownsFile) {
      if (fclose(lock->file) != 0) {
        return push_error(L, "unlock");
      }
      lock->file = NULL;
      lock->ownsFile = 0;
    } else {
      if (!_file_lock(L, lock->file, "u", lock->start, lock->len, "unlock")) {
        return push_error(L, NULL);
      }
      lock->file = NULL;
    }
  }
  lua_pushboolean(L, 1);
  return 1;
}

int eli_is_lock_active(lua_State *L) {
 efs_lock *lock = (efs_lock *)luaL_checkudata(L, 1, LOCK_METATABLE);
 lua_pushboolean(L, lock->file != NULL);
  return 1;
}

#ifdef _WIN32
typedef struct efs_dir_lock
{
    HANDLE fd;
} efs_dir_lock;
int eli_lock_dir(lua_State *L)
{
    size_t pathl;
    HANDLE fd;
    efs_dir_lock *lock;
    char *ln;
    const char *path = luaL_checklstring(L, 1, &pathl);
    const char *lockfile = luaL_optstring(L, 2, "lockfile");
    ln = joinpath(path, lockfile);
    if (!ln)
    {
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        return 2;
    }
    fd = CreateFile(ln, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    free(ln);
    if (fd == INVALID_HANDLE_VALUE) {
        return windows_pushlasterror(L);
    }
    lock = (efs_dir_lock *)lua_newuserdata(L, sizeof(efs_dir_lock));
    lock->fd = fd;
    luaL_getmetatable(L, LOCK_DIR_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}
int eli_unlock_dir(lua_State *L)
{
    efs_dir_lock *lock = (efs_dir_lock *)luaL_checkudata(L, 1, LOCK_DIR_METATABLE);
    if (lock->fd != INVALID_HANDLE_VALUE)
    {
        CloseHandle(lock->fd);
        lock->fd = INVALID_HANDLE_VALUE;
    }
    return 0;
}

int eli_is_dir_lock_active(lua_State *L) 
{
  efs_dir_lock *lock = (efs_dir_lock *)luaL_checkudata(L, 1, LOCK_DIR_METATABLE);
  lua_pushboolean(L, lock->fd != INVALID_HANDLE_VALUE);
  return 1;
}
#else
typedef struct efs_dir_lock
{
    char *ln;
} efs_dir_lock;

int eli_lock_dir(lua_State *L)
{
    efs_dir_lock *lock;
    size_t pathl;
    char *ln;
    const char *path = luaL_checklstring(L, 1, &pathl);
    const char *lockfile = luaL_optstring(L, 2, "lockfile");
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
    lock = (efs_dir_lock *)lua_newuserdata(L, sizeof(efs_dir_lock));
    lock->ln = ln;

    luaL_getmetatable(L, LOCK_DIR_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}

int eli_unlock_dir(lua_State *L)
{
    efs_dir_lock *lock = (efs_dir_lock *)luaL_checkudata(L, 1, LOCK_DIR_METATABLE);
    if (lock->ln)
    {
        if (unlink(lock->ln) != 0) {
            lua_pushnil(L);
            lua_pushstring(L, strerror(errno));
            return 2;
        }
        free(lock->ln);
        lock->ln = NULL;
    }
    lua_pushboolean(L, 1);
    return 1;
}

int eli_is_dir_lock_active(lua_State *L) 
{
  efs_dir_lock *lock = (efs_dir_lock *)luaL_checkudata(L, 1, LOCK_DIR_METATABLE);
  lua_pushboolean(L, lock->ln != NULL);
  return 1;
}
#endif

/*
** Creates lock metatable.
*/
int dir_lock_create_meta(lua_State *L)
{
    luaL_newmetatable(L, LOCK_DIR_METATABLE);
    /* Method table */
    lua_newtable(L);
    lua_pushcfunction(L, eli_unlock_dir);
    lua_setfield(L, -2, "free");
    lua_pushcfunction(L, eli_unlock_dir);
    lua_setfield(L, -2, "unlock");
    lua_pushcfunction(L, eli_is_dir_lock_active);
    lua_setfield(L, -2, "is_active");
    /* type */
    lua_pushstring(L, LOCK_DIR_METATABLE);
    lua_setfield(L, -2, "__type");
    /* Metamethods */
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, eli_unlock_dir);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, eli_unlock_dir);
    lua_setfield(L, -2, "__close");
    return 1;
}

int lock_create_meta(lua_State *L) {
    luaL_newmetatable(L, LOCK_METATABLE);
    /* Method table */
    lua_newtable(L);
    lua_pushcfunction(L, eli_file_unlock);
    lua_setfield(L, -2, "free");
    lua_pushcfunction(L, eli_file_unlock);
    lua_setfield(L, -2, "unlock");
    lua_pushcfunction(L, eli_is_lock_active);
    lua_setfield(L, -2, "is_active");
    /* type */
    lua_pushstring(L, LOCK_METATABLE);
    lua_setfield(L, -2, "__type");

    /* Metamethods */
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, eli_file_unlock);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, eli_file_unlock);
    lua_setfield(L, -2, "__close");
    return 1;
}
