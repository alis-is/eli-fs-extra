#ifndef ELI_EXTRA_FS_UTIL_H__
#define ELI_EXTRA_FS_UTIL_H__

#include "lua.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

char *joinpath(const char *pth1, const char *pth2);
FILE *check_file(lua_State *L, int idx, const char *funcname);

#ifdef _WIN32
const char *mode2string(unsigned short mode);
const char *perm2string(unsigned short mode);
#else
const char *mode2string(mode_t mode);
const char *perm2string(mode_t mode);
#endif

char *clone_string(const char *str);

#endif /* ELI_EXTRA_FS_UTIL_H__ */