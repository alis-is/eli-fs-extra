#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include <errno.h>
#include "lerror.h"
#include "lfsutil.h"

#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32

#include <io.h>

#define LMODE_T int

#define _lchmod _chmod

#else // unix

#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#define LMODE_T mode_t

#define _lchmod chmod
#endif

int eli_chmod(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	LMODE_T mode;
	switch (lua_type(L, 2)) {
	case LUA_TNUMBER: {
		mode = luaL_checknumber(L, 2);
		break;
	}
	case LUA_TSTRING: {
		size_t len;
		const char *smode = lua_tolstring(L, 2, &len);
		if (len < 9) {
			return luaL_argerror(
				L, 2,
				"Mode as string has to be at least 9 characters long.");
		}
		mode = 0;
#ifdef _WIN32
		if (smode[0] == 'r' || smode[3] == 'r' || smode[6] == 'r')
			mode |= _S_IREAD;
		if (smode[1] == 'w' || smode[4] == 'w' || smode[7] == 'w')
			mode |= _S_IWRITE;
		if (smode[2] == 'x' || smode[5] == 'x' || smode[8] == 'x')
			mode |= _S_IEXEC;
#else
		if (smode[0] == 'r')
			mode |= S_IRUSR;
		if (smode[1] == 'w')
			mode |= S_IWUSR;
		if (smode[2] == 'x')
			mode |= S_IXUSR;
		if (smode[3] == 'r')
			mode |= S_IRGRP;
		if (smode[4] == 'w')
			mode |= S_IWGRP;
		if (smode[5] == 'x')
			mode |= S_IXGRP;
		if (smode[6] == 'r')
			mode |= S_IROTH;
		if (smode[7] == 'w')
			mode |= S_IWOTH;
		if (smode[8] == 'x')
			mode |= S_IXOTH;
#endif
		break;
	}
	default:
		return luaL_typeerror(L, 2, "mode has to be string or number");
	}
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

int eli_getuid(lua_State *L)
{
#ifdef _WIN32
	errno = ENOSYS; /* = "Function not implemented" */
	return push_result(L, -1, "getuid is not supported on Windows");
#else
	const char *user = luaL_checkstring(L, 1);
	struct passwd *p;
	if ((p = getpwnam(user)) == NULL) {
		return push_error(L, NULL);
	}
	lua_pushinteger(L, p->pw_uid);
	return 1;
#endif
}

int eli_getgid(lua_State *L)
{
#ifdef _WIN32
	errno = ENOSYS; /* = "Function not implemented" */
	return push_result(L, -1, "getuid is not supported on Windows");
#else
	const char *group = luaL_checkstring(L, 1);
	struct group *g;
	if ((g = getgrnam(group)) == NULL) {
		return push_error(L, NULL);
	}
	lua_pushinteger(L, g->gr_gid);
	return 1;
#endif
}