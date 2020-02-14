#include "lua.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

char *joinpath(const char *pth1, const char *pth2);
int push_error(lua_State *L, const char *info);
FILE *check_file(lua_State *L, int idx, const char *funcname);
int push_result(lua_State *L, int res, const char *info);

#ifdef _WIN32
const char *mode2string(unsigned short mode);
const char *perm2string(unsigned short mode);
#else
const char *mode2string(mode_t mode);
const char *perm2string(mode_t mode);
#endif

char *clone_string(char * restrict str);