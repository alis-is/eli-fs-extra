#ifndef ELI_EXTRA_FS_PERM_H__
#define ELI_EXTRA_FS_PERM_H__

#include "lua.h"

int eli_chmod(lua_State *L);
int eli_chown(lua_State *L);
int eli_getuid(lua_State *L);
int eli_getgid(lua_State *L);

#endif /* ELI_EXTRA_FS_PERM_H__ */