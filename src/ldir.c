#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lutil.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32

#include <direct.h>
#include <windows.h>
#include <io.h>

#else // unix

#include <dirent.h>
#include <stdio.h>
#include <unistd.h>

#ifndef MAXPATHLEN
#include <limits.h> /* for _POSIX_PATH_MAX */
#endif

#endif

#define DIR_METATABLE "ELI_DIR"
#define DIR_ENTRY_METATABLE "ELI_DIRENTRY"

#ifdef _WIN32

#define DIR_SEPARATOR '\\'
#define LMAXPATHLEN MAX_PATH
#define STAT_STRUCT struct _stati64
#define STAT_FUNC _stati64

#define _lmkdir _mkdir

#else // unix

#define DIR_SEPARATOR '/'
#ifdef MAXPATHLEN
#define LMAXPATHLEN MAXPATHLEN
#else
#define LMAXPATHLEN _POSIX_PATH_MAX
#endif
#define STAT_STRUCT struct stat
#define STAT_FUNC stat

#define _lmkdir(path) (mkdir((path), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH))

#endif

typedef struct dir_data
{
    int closed;
    const char *path;
#ifdef _WIN32
    intptr_t hFile;
    char pattern[LMAXPATHLEN + 1];
#else
    DIR *dir;
#endif
    int withFileTypes;
} dir_data;

typedef struct dir_entry_data
{
    const char *folder;
#ifdef _WIN32
    struct _finddata_t c_file;
#else
    struct dirent *entry;
#endif
} dir_entry_data;

/*
** Creates a directory.
** @param {string} directory path.
*/
int eli_mkdir(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    return push_result(L, _lmkdir(path), NULL);
}

int eli_rmdir(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    return push_result(L, rmdir(path), NULL);
}

static int isdotfile(const char *name)
{
    return name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

int eli_read_dir(lua_State *L)
{
#ifdef _WIN32
    struct _finddata_t c_file;
#else
    struct dirent *entry;
#endif

    const char *path = luaL_checkstring(L, 1);
    const int withFileTypes = lua_toboolean(L, 2);
    lua_newtable(L);
    int resultPosition = lua_gettop(L);
#ifdef _WIN32
    intptr_t hFile = 0L;
    char pattern[LMAXPATHLEN + 1];
    if (strlen(path) > LMAXPATHLEN - 2)
        return luaL_error(L, "path too long: %s", path);
    else
        sprintf(pattern, "%s/*", path);

    int i = 1;
    if ((hFile = _findfirst(pattern, &c_file)) == -1L)
    {
        return 1;
    }
    else if (!isdotfile(c_file.name))
    {
        if (withFileTypes)
        {
            struct dir_entry_data *result = lua_newuserdata(L, sizeof(struct dir_entry_data));
            memcpy(&result->c_file, &c_file, sizeof(struct _finddata_t));
            result->folder = path;
            luaL_getmetatable(L, DIR_ENTRY_METATABLE);
            lua_setmetatable(L, -2);
            lua_rawseti(L, resultPosition, i++); /* t[i] = result */
        }
        else
        {
            lua_pushstring(L, c_file.name);      /* push path */
            lua_rawseti(L, resultPosition, i++); /* t[i] = result */
        }
    }

    while (_findnext(hFile, &c_file) != -1L)
    {
        if (isdotfile(c_file.name))
            continue;
        if (withFileTypes)
        {
            struct dir_entry_data *result = lua_newuserdata(L, sizeof(struct dir_entry_data));
            memcpy(&result->c_file, &c_file, sizeof(struct _finddata_t));
            result->folder = path;
            luaL_getmetatable(L, DIR_ENTRY_METATABLE);
            lua_setmetatable(L, -2);
            lua_rawseti(L, resultPosition, i++); /* t[i] = result */
        }
        else
        {
            lua_pushstring(L, c_file.name);      /* push path */
            lua_rawseti(L, resultPosition, i++); /* t[i] = result */
        }
    }
    _findclose(hFile);
#else
    DIR *dir = opendir(path);
    if (dir == NULL)
        return luaL_error(L, "cannot open %s: %s", path, strerror(errno));

    int i = 1;
    while ((entry = readdir(dir)) != NULL)
    {
        if (isdotfile(entry->d_name))
            continue;
        if (withFileTypes)
        {
            struct dir_entry_data *result = lua_newuserdata(L, sizeof(struct dir_entry_data));
            result->entry = entry;
            result->folder = path;
            luaL_getmetatable(L, DIR_ENTRY_METATABLE);
            lua_setmetatable(L, -2);
        }
        else
        {
            lua_pushstring(L, entry->d_name);    /* push path */
            lua_rawseti(L, resultPosition, i++); /* t[i] = result */
        }
    }
    closedir(dir);
#endif
    return 1;
}

/*
** Directory iterator
*/
static int dir_iter(lua_State *L)
{
#ifdef _WIN32
    struct _finddata_t c_file;
#else
    struct dirent *entry;
#endif
    dir_data *d = (dir_data *)luaL_checkudata(L, 1, DIR_METATABLE);
    luaL_argcheck(L, d->closed == 0, 1, "closed directory");
    const int withFileTypes = withFileTypes == -1 ? lua_toboolean(L, 2) : d->withFileTypes;
#ifdef _WIN32
    if (d->hFile == 0L)
    { /* first entry */
        if ((d->hFile = _findfirst(d->pattern, &c_file)) == -1L)
        {
            lua_pushnil(L);
            lua_pushstring(L, strerror(errno));
            d->closed = 1;
            return 2;
        }
        else if (!isdotfile(c_file.name))
        {
            if (withFileTypes)
            {
                struct dir_entry_data *result = lua_newuserdata(L, sizeof(struct dir_entry_data));
                memcpy(&result->c_file, &c_file, sizeof(struct _finddata_t));
                result->folder = d->path;
                luaL_getmetatable(L, DIR_ENTRY_METATABLE);
                lua_setmetatable(L, -2);
            }
            else
            {
                lua_pushstring(L, c_file.name);
            }
            return 1;
        }
    }
    // skip dot files
    long found;
    while ((found = _findnext(d->hFile, &c_file)) != -1L && isdotfile(c_file.name))
        continue;

    /* next entry */
    if (found == -1L)
    {
        /* no more entries => close directory */
        _findclose(d->hFile);
        d->closed = 1;
        return 0;
    }
    else
    {
        if (withFileTypes)
        {
            struct dir_entry_data *result = lua_newuserdata(L, sizeof(struct dir_entry_data));
            memcpy(&result->c_file, &c_file, sizeof(struct _finddata_t));
            result->folder = d->path;
            luaL_getmetatable(L, DIR_ENTRY_METATABLE);
            lua_setmetatable(L, -2);
        }
        else
        {
            lua_pushstring(L, c_file.name);
        }
        return 1;
    }

#else
    // skip dot files
    while ((entry = readdir(d->dir)) != NULL && isdotfile(entry->d_name))
        continue;

    if (entry != NULL)
    {

        if (withFileTypes)
        {
            struct dir_entry_data *result = lua_newuserdata(L, sizeof(struct dir_entry_data));
            result->entry = entry;
            result->folder = d->path;
            luaL_getmetatable(L, DIR_ENTRY_METATABLE);
            lua_setmetatable(L, -2);
        }
        else
        {
            lua_pushstring(L, entry->d_name);
        }
        return 1;
    }
    else
    {
        /* no more entries => close directory */
        closedir(d->dir);
        d->closed = 1;
        return 0;
    }
#endif
}

/*
** Factory of directory iterators
*/
int eli_open_dir(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    dir_data *d;

    d = (dir_data *)lua_newuserdata(L, sizeof(dir_data));
    luaL_getmetatable(L, DIR_METATABLE);
    lua_setmetatable(L, -2);
    d->closed = 0;
    d->path = path;
    d->withFileTypes = -1;
#ifdef _WIN32
    d->hFile = 0L;
    if (strlen(path) > LMAXPATHLEN - 2)
        return luaL_error(L, "path too long: %s", path);
    else
        sprintf(d->pattern, "%s/*", path);
#else
    d->dir = opendir(path);
    if (d->dir == NULL)
        return luaL_error(L, "cannot open %s: %s", path, strerror(errno));
#endif
    return 1;
}

int eli_iter_dir(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int withFileTypes = lua_toboolean(L, 2);
    dir_data *d;

    lua_pushcfunction(L, dir_iter);
    d = (dir_data *)lua_newuserdata(L, sizeof(dir_data));
    luaL_getmetatable(L, DIR_METATABLE);
    lua_setmetatable(L, -2);
    d->closed = 0;
    d->withFileTypes = withFileTypes;
    d->path = path;
#ifdef _WIN32
    d->hFile = 0L;
    if (strlen(path) > LMAXPATHLEN - 2)
        return luaL_error(L, "path too long: %s", path);
    else
        sprintf(d->pattern, "%s/*", path);
#else
    d->dir = opendir(path);
    if (d->dir == NULL)
        return luaL_error(L, "cannot open %s: %s", path, strerror(errno));
#endif
    return 2;
}

/*
** Closes directory iterators
*/
static int lclosedir(lua_State *L)
{
    dir_data *d = (dir_data *)lua_touserdata(L, 1);
#ifdef _WIN32
    if (!d->closed && d->hFile)
    {
        _findclose(d->hFile);
    }
#else
    if (!d->closed && d->dir)
    {
        closedir(d->dir);
    }
#endif
    d->closed = 1;
    return 0;
}

static int dir_path(lua_State *L)
{
    dir_data *d = (dir_data *)luaL_checkudata(L, 1, DIR_METATABLE);
    luaL_argcheck(L, d->closed == 0, 1, "closed directory");
    lua_pushstring(L, d->path);
    return 1;
}

/*
** Creates directory metatable.
*/
int dir_create_meta(lua_State *L)
{
    luaL_newmetatable(L, DIR_METATABLE);

    /* Method table */
    lua_newtable(L);
    lua_pushcfunction(L, dir_iter);
    lua_setfield(L, -2, "next");
    lua_pushcfunction(L, lclosedir);
    lua_setfield(L, -2, "close");
    lua_pushcfunction(L, dir_path);
    lua_setfield(L, -2, "path");
    lua_pushstring(L, DIR_METATABLE);
    lua_setfield(L, -2, "__type");

    /* Metamethods */
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, lclosedir);
    lua_setfield(L, -2, "__gc");
    return 1;
}

int dir_entry_type(lua_State *L)
{
#ifdef _WIN32
    struct dir_entry_data *ded = (struct dir_entry_data *)luaL_checkudata(L, 1, DIR_ENTRY_METATABLE);
    char *path = joinpath(ded->folder, ded->c_file.name);
#else
    struct dir_entry_data *ded = (struct dir_entry_data *)luaL_checkudata(L, 1, DIR_ENTRY_METATABLE);
    char *path = joinpath(ded->folder, ded->entry->d_name);
#endif
    if (!path)
    {
        push_error(L, "Out of memory");
    }

    STAT_STRUCT info;
    if (STAT_FUNC(path, &info))
    {
        lua_pushnil(L);
        lua_pushfstring(L, "cannot obtain information from path '%s': %s", path, strerror(errno));
        lua_pushinteger(L, errno);
        return 3;
    }
    free(path);
    lua_pushstring(L, mode2string(info.st_mode));
    return 1;
}

static int dir_entry_name(lua_State *L)
{
#ifdef _WIN32
    struct dir_entry_data *ded = (struct dir_entry_data *)luaL_checkudata(L, 1, DIR_ENTRY_METATABLE);
    lua_pushstring(L, ded->c_file.name);
#else
    struct dir_entry_data *ded = (struct dir_entry_data *)luaL_checkudata(L, 1, DIR_ENTRY_METATABLE);
    lua_pushstring(L, ded->entry->d_name);
#endif
    return 1;
}

static int dir_entry_fullpath(lua_State *L)
{
#ifdef _WIN32
    struct dir_entry_data *ded = (struct dir_entry_data *)luaL_checkudata(L, 1, DIR_ENTRY_METATABLE);
    char *res = joinpath(ded->folder, ded->c_file.name);

#else
    struct dir_entry_data *ded = (struct dir_entry_data *)luaL_checkudata(L, 1, DIR_ENTRY_METATABLE);
    char *res = joinpath(ded->folder, ded->entry->d_name);
#endif
    if (!res)
    {
        push_error(L, "Out of memory");
    }

    lua_pushstring(L, res);
    free(res);
    return 1;
}

/*
** Creates directory metatable.
*/
int direntry_create_meta(lua_State *L)
{
    luaL_newmetatable(L, DIR_ENTRY_METATABLE);

    /* Method table */
    lua_newtable(L);
    lua_pushcfunction(L, dir_entry_name);
    lua_setfield(L, -2, "name");
    lua_pushcfunction(L, dir_entry_type);
    lua_setfield(L, -2, "type");
    lua_pushcfunction(L, dir_entry_fullpath);
    lua_setfield(L, -2, "fullpath");

    lua_pushstring(L, DIR_ENTRY_METATABLE);
    lua_setfield(L, -2, "__type");

    /* Metamethods */
    lua_setfield(L, -2, "__index");
    return 1;
}
