#include "lua.h"
#include "lauxlib.h"
#include "lutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static int io_fclose(lua_State *L)
{
    luaL_Stream *p = ((luaL_Stream *)luaL_checkudata(L, 1, LUA_FILEHANDLE));
    int res = fclose(p->f);
    return luaL_fileresult(L, (res == 0), NULL);
}
static luaL_Stream *new_file(lua_State *L, int fd, const char *mode)
{
    luaL_Stream *p = (luaL_Stream *)lua_newuserdata(L, sizeof(luaL_Stream));
    luaL_setmetatable(L, LUA_FILEHANDLE);
    p->f = fdopen(fd, mode);
    p->closef = &io_fclose;
    return p;
}

#ifndef _WIN32
static int closeonexec(int d)
{
    int fl = fcntl(d, F_GETFD);
    if (fl != -1)
        fl = fcntl(d, F_SETFD, fl | FD_CLOEXEC);
    return fl;
}
#endif

/* -- in out/nil error */
int eli_pipe(lua_State *L)
{
#ifdef _WIN32
    HANDLE ph[2];
    if (!CreatePipe(ph + 0, ph + 1, 0, 0))
        return push_error(L, NULL);
    SetHandleInformation(ph[0], HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(ph[1], HANDLE_FLAG_INHERIT, 0);
    new_file(L, ph[0], _O_RDONLY, "r");
    new_file(L, ph[1], _O_WRONLY, "w");
#else
    int fd[2];
    if (-1 == pipe(fd))
        return push_error(L, NULL);
    closeonexec(fd[0]);
    closeonexec(fd[1]);
    new_file(L, fd[0], "r");
    new_file(L, fd[1], "w");
#endif
    return 2;
}