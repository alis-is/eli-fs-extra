#include <errno.h>
#include "lua.h"
#include <string.h>
#include "lauxlib.h"
#include <sys/stat.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>

char *joinpath(char *pth1, char *pth2)
{
    if (pth1 == NULL && pth2 == NULL)
    {
        return "";
    }
    else if (pth2 == NULL || strlen(pth2) == 0)
    {
        return pth1;
    }
    else if (pth1 == NULL || strlen(pth1) == 0)
    {
        return pth2;
    }
    else
    {
#ifdef _WIN32
        char directory_separator[] = "\\";
#else
        char directory_separator[] = "/";
#endif
        const char *last_char = pth1;
        int append_directory_separator = 0;
        if (pth1[strlen(pth1) - 1] == directory_separator[0])
        {
            char *dst = malloc(strlen(pth1) + strlen(pth2));
            if (!dst) {
                return "";
            }
            strcpy(dst, pth1);
            strcat(dst, pth2);
        }
        else
        {
            char *dst = malloc(strlen(pth1) + strlen(pth2) + strlen(directory_separator));
            if (!dst) {
                return "";
            }
            strcpy(dst, pth1);
            strcat(dst, directory_separator);
            strcat(dst, pth2);
        }
    }
}

int push_error(lua_State *L, const char *info)
{
    lua_pushnil(L);
    if (info == NULL)
        lua_pushstring(L, strerror(errno));
    else
        lua_pushfstring(L, "%s: %s", info, strerror(errno));
    lua_pushinteger(L, errno);
    return 3;
}

int push_result(lua_State *L, int res, const char *info)
{
    if (res == -1)
    {
        return push_error(L, info);
    }
    else
    {
        lua_pushboolean(L, 1);
        return 1;
    }
}

/*
** Check if the given element on the stack is a file and returns it.
*/
FILE *check_file(lua_State *L, int idx, const char *funcname)
{
    luaL_Stream *fh = (luaL_Stream *)luaL_checkudata(L, idx, "FILE*");
    if (fh->closef == 0 || fh->f == NULL)
    {
        luaL_error(L, "%s: closed file", funcname);
        return NULL;
    }
    else
    {
        return fh->f;
    }
}

#ifdef _WIN32
#ifndef S_ISDIR
#define S_ISDIR(mode) (mode & _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(mode) (mode & _S_IFREG)
#endif
#ifndef S_ISLNK
#define S_ISLNK(mode) (0)
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(mode) (0)
#endif
#ifndef S_ISFIFO
#define S_ISFIFO(mode) (0)
#endif
#ifndef S_ISCHR
#define S_ISCHR(mode) (mode & _S_IFCHR)
#endif
#ifndef S_ISBLK
#define S_ISBLK(mode) (0)
#endif
#endif
/*
** Convert the inode protection mode to a string.
*/
#ifdef _WIN32
const char *mode2string(unsigned short mode)
{
#else
const char *mode2string(mode_t mode)
{
#endif
    if (S_ISREG(mode))
        return "file";
    else if (S_ISDIR(mode))
        return "directory";
    else if (S_ISLNK(mode))
        return "link";
    else if (S_ISSOCK(mode))
        return "socket";
    else if (S_ISFIFO(mode))
        return "named pipe";
    else if (S_ISCHR(mode))
        return "char device";
    else if (S_ISBLK(mode))
        return "block device";
    else
        return "other";
}

#ifdef _WIN32
const char *perm2string(unsigned short mode)
{
    static char perms[10] = "---------";
    if (mode & _S_IREAD)
    {
        perms[0] = 'r';
        perms[3] = 'r';
        perms[6] = 'r';
    }
    if (mode & _S_IWRITE)
    {
        perms[1] = 'w';
        perms[4] = 'w';
        perms[7] = 'w';
    }
    if (mode & _S_IEXEC)
    {
        perms[2] = 'x';
        perms[5] = 'x';
        perms[8] = 'x';
    }
    return perms;
}
#else
const char *perm2string(mode_t mode)
{
    static char perms[10] = "---------";
    if (mode & S_IRUSR)
        perms[0] = 'r';
    if (mode & S_IWUSR)
        perms[1] = 'w';
    if (mode & S_IXUSR)
        perms[2] = 'x';
    if (mode & S_IRGRP)
        perms[3] = 'r';
    if (mode & S_IWGRP)
        perms[4] = 'w';
    if (mode & S_IXGRP)
        perms[5] = 'x';
    if (mode & S_IROTH)
        perms[6] = 'r';
    if (mode & S_IWOTH)
        perms[7] = 'w';
    if (mode & S_IXOTH)
        perms[8] = 'x';
    return perms;
}
#endif
