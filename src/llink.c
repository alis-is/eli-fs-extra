#include "lua.h"
#include "lauxlib.h"

#include "lutil.h"
#include "lfsutil.h"
#include "lfile.h"

#ifdef _WIN32

#include <windows.h>

#else // unix

#include <unistd.h>

#endif

/*
** Creates a link.
** @param #1 Object to link to.
** @param #2 Name of link.
** @param #3 True if link is symbolic (optional).
*/
int eli_mklink(lua_State *L)
{
    const char *origin = luaL_checkstring(L, 1);
    const char *target = luaL_checkstring(L, 2);
#ifndef _WIN32

    int res = (lua_toboolean(L, 3) ? symlink : link)(origin, target);
    if (res == -1)
    {
        return push_error(L, NULL);
    }
    else
    {
        return 0;
    }
#else
    int symlink = lua_toboolean(L, 3);
    if (symlink)
    {
        const char *type;
        if (_file_type(target, &type))
        {
            lua_pushnil(L);
            lua_pushfstring(L, "cannot obtain information from path '%s': %s", target, strerror(errno));
            lua_pushinteger(L, errno);
            return 3;
        }
        DWORD creationFlag = strcmp(type, "directory") == 0 ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
        if (CreateSymbolicLink(target, origin, creationFlag))
        {
            return 0;
        }
        else
        {
            return push_error(L, "Failed to create symbolic link");
        }
    }
    else
    {
        if (CreateHardLink(target,
                           origin,
                           NULL))
        {
            return 0;
        }
        else
        {
            return push_error(L, "Failed to create hardlink");
        }
    }
#endif
}
