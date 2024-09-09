#include "lua.h"
#include "lauxlib.h"

#include "lutil.h"
#include "lfsutil.h"
#include "lfile.h"

#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define TYPE_CHECK_FILE 0
#define TYPE_CHECK_LINK 1

#ifdef _WIN32

#include <windows.h>
#include <stdio.h>
#include <sys/utime.h>
#include <fcntl.h>

#else // unix

#include <stdio.h>
#include <unistd.h>
#include <utime.h>
#include <fcntl.h>

#endif

#ifndef _S_IFLNK
#define _S_IFLNK 0x400
#endif

#ifdef _WIN32

#define DIR_SEPARATOR '\\'
#define STAT_STRUCT struct _stati64
#define STAT_FUNC _stati64
#define FSTAT_FUNC _fstati64
#define LSTAT_FUNC lfs_win32_lstat
#define LMAXPATHLEN MAX_PATH

#define _lsetmode(file, m) (_setmode(_fileno(file), m))

#else // unix

#define DIR_SEPARATOR '/'
#define _O_TEXT 0
#define _O_BINARY 0
#define _lsetmode(file, m) ((void)file, (void)m, 0)
#define STAT_STRUCT struct stat
#define STAT_FUNC stat
#define FSTAT_FUNC fstat
#define LSTAT_FUNC lstat

#endif

struct _stat_members {
	const char *name;
	int id;
};

struct _stat_members members[] = { { "mode", 0 },	  { "type", 0 },
				   { "dev", 1 },	  { "ino", 2 },
				   { "nlink", 3 },	  { "uid", 4 },
				   { "gid", 5 },	  { "rdev", 6 },
				   { "access", 7 },	  { "modification", 8 },
				   { "change", 9 },	  { "size", 10 },
				   { "permissions", 11 },
#ifndef _WIN32
				   { "blocks", 12 },	  { "blksize", 13 },
#endif
				   { NULL, -1 } };

static int _push_file_info_member(lua_State *L, STAT_STRUCT *info,
				  const char *member, int memberId)
{
	if (memberId < 0) {
		int i;
		for (i = 0; members[i].name != NULL; i++) {
			if (strcmp(members[i].name, member) == 0) {
				memberId = members[i].id;
				break;
			}
		}
	}
	switch (memberId) {
	case 0:
		lua_pushstring(L, mode2string(info->st_mode));
		return 1;
	case 1:
		lua_pushinteger(L, (lua_Integer)info->st_dev);
		return 1;
	case 2:
		lua_pushinteger(L, (lua_Integer)info->st_ino);
		return 1;
	case 3:
		lua_pushinteger(L, (lua_Integer)info->st_nlink);
		return 1;
	case 4:
		lua_pushinteger(L, (lua_Integer)info->st_uid);
		return 1;
	case 5:
		lua_pushinteger(L, (lua_Integer)info->st_gid);
		return 1;
	case 6:
		lua_pushinteger(L, (lua_Integer)info->st_rdev);
		return 1;
	case 7:
		lua_pushinteger(L, (lua_Integer)info->st_atime);
		return 1;
	case 8:
		lua_pushinteger(L, (lua_Integer)info->st_mtime);
		return 1;
	case 9:
		lua_pushinteger(L, (lua_Integer)info->st_ctime);
		return 1;
	case 10:
		lua_pushinteger(L, (lua_Integer)info->st_size);
		return 1;
	case 11:
		lua_pushstring(L, perm2string(info->st_mode));
		return 1;
#ifndef _WIN32
	case 12:
		lua_pushinteger(L, (lua_Integer)info->st_blocks);
		return 1;
	case 13:
		lua_pushinteger(L, (lua_Integer)info->st_blksize);
		return 1;
#endif
	default:
		return luaL_error(L, "invalid attribute name '%s'", member);
	}
}

static int file_info(lua_State *L, int file_type_check)
{
	STAT_STRUCT info;
	const char *file;
	if (lua_isstring(L, 1)) {
		file = luaL_checkstring(L, 1);
		int result = 0;
		switch (file_type_check) {
		case TYPE_CHECK_FILE:
			result = STAT_FUNC(file, &info);
			break;
		case TYPE_CHECK_LINK:
			result = LSTAT_FUNC(file, &info);
			break;
		}
		if (result) {
			lua_pushnil(L);
			lua_pushfstring(
				L,
				"cannot obtain information from file '%s': %s",
				file, strerror(errno));
			lua_pushinteger(L, errno);
			return 3;
		}
	} else if (lua_isuserdata(L, 1)) {
		FILE *f = *(FILE **)luaL_checkudata(L, 1, LUA_FILEHANDLE);
		if (f == NULL) {
			return luaL_argerror(L, 1, "file is closed");
		}
		if (FSTAT_FUNC(fileno(f), &info)) {
			return push_error(
				L, "cannot obtain information from file");
		}
	} else {
		return luaL_argerror(L, 1, "expected string or file*");
	}

	if (lua_isstring(L, 2)) {
		const char *member = lua_tostring(L, 2);
		_push_file_info_member(L, &info, member, -1);
	}
	/* creates a table if none is given, removes extra arguments */
	lua_settop(L, 2);
	if (!lua_istable(L, 2)) {
		lua_newtable(L);
	}
	/* stores all members in table on top of the stack */
	for (int i = 0; members[i].name != NULL; i++) {
		lua_pushstring(L, members[i].name);
		_push_file_info_member(L, &info, members[i].name,
				       members[i].id);
		lua_rawset(L, -3);
	}
	return 1;
}

/*
** Get file information
*/
int eli_file_info(lua_State *L)
{
	return file_info(L, TYPE_CHECK_FILE);
}

#ifdef _WIN32
#define TICKS_PER_SECOND 10000000
#define EPOCH_DIFFERENCE 11644473600LL
time_t windowsToUnixTime(FILETIME ft)
{
	ULARGE_INTEGER uli;
	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;
	return (time_t)(uli.QuadPart / TICKS_PER_SECOND - EPOCH_DIFFERENCE);
}

static int lfs_win32_lstat(const char *path, STAT_STRUCT *buffer)
{
	WIN32_FILE_ATTRIBUTE_DATA win32buffer;
	if (GetFileAttributesEx(path, GetFileExInfoStandard, &win32buffer)) {
		if (!(win32buffer.dwFileAttributes &
		      FILE_ATTRIBUTE_REPARSE_POINT)) {
			return STAT_FUNC(path, buffer);
		}
		buffer->st_mode = _S_IFLNK;
		buffer->st_dev = 0;
		buffer->st_ino = 0;
		buffer->st_nlink = 0;
		buffer->st_uid = 0;
		buffer->st_gid = 0;
		buffer->st_rdev = 0;
		buffer->st_atime =
			windowsToUnixTime(win32buffer.ftLastAccessTime);
		buffer->st_mtime =
			windowsToUnixTime(win32buffer.ftLastWriteTime);
		buffer->st_ctime =
			windowsToUnixTime(win32buffer.ftCreationTime);
		buffer->st_size = 0;
		return 0;
	} else {
		return 1;
	}
}
#endif

static int push_link_target(lua_State *L)
{
#ifdef _WIN32
	const char *file = luaL_checkstring(L, 1);
	HANDLE h = CreateFile(file, FILE_READ_ATTRIBUTES,
			      FILE_SHARE_READ | FILE_SHARE_WRITE |
				      FILE_SHARE_DELETE,
			      0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
	if (h == INVALID_HANDLE_VALUE) {
		return push_error(L, "Failed to open link");
	}
	char path[LMAXPATHLEN];
	DWORD dwRet =
		GetFinalPathNameByHandle(h, path, LMAXPATHLEN, VOLUME_NAME_NT);
	if (dwRet == 0) {
		return push_error(L, "Failed to get link target");
	}
	CloseHandle(h);
	lua_pushlstring(L, path, dwRet);
	return 1;
#else
	const char *file = luaL_checkstring(L, 1);
	char *target = NULL;
	int tsize, size = 256; /* size = initial buffer capacity */
	while (1) {
		char *target2 = realloc(target, size);
		if (!target2) { /* failed to allocate */
			free(target);
			return 0;
		}
		target = target2;
		tsize = readlink(file, target, size);
		if (tsize < 0) { /* a readlink() error occurred */
			free(target);
			return 0;
		}
		if (tsize < size)
			break;
		/* possibly truncated readlink() result, double size and retry */
		size *= 2;
	}
	target[tsize] = '\0';
	lua_pushlstring(L, target, tsize);
	free(target);
	return 1;
#endif
}

/*
** Get symbolic link information using lstat.
*/
int eli_link_info(lua_State *L)
{
	int ret;
	if (lua_isstring(L, 2) && (strcmp(lua_tostring(L, 2), "target") == 0)) {
		int ok = push_link_target(L);
		return ok ? 1 : push_error(L, "could not obtain link target");
	}
	ret = file_info(L, TYPE_CHECK_LINK);
	if (ret == 1 && lua_type(L, -1) == LUA_TTABLE) {
		int ok = push_link_target(L);
		if (ok) {
			lua_setfield(L, -2, "target");
		}
	}
	return ret;
}

int eli_set_file_mode(lua_State *L)
{
	FILE *f = check_file(L, 1, "setmode");
	static const int mode[] = { _O_BINARY, _O_TEXT };
	static const char *const modenames[] = { "binary", "text", NULL };
	int op = luaL_checkoption(L, 2, NULL, modenames);
	int res = _lsetmode(f, mode[op]);
	if (res != -1) {
		int i;
		lua_pushboolean(L, 1);
		for (i = 0; modenames[i] != NULL; i++) {
			if (mode[i] == res) {
				lua_pushstring(L, modenames[i]);
				return 2;
			}
		}
		lua_pushnil(L);
		return 2;
	} else {
		return push_error(L, NULL);
	}
}

/*
** Set access time and modification values for a file.
** @param #1 File path.
** @param #2 Access time in seconds, current time is used if missing.
** @param #3 Modification time in seconds, access time is used if missing.
*/
int eli_file_utime(lua_State *L)
{
	const char *file = luaL_checkstring(L, 1);
	struct utimbuf utb, *buf;

	if (lua_gettop(L) == 1) /* set to current date/time */
		buf = NULL;
	else {
		utb.actime = (time_t)luaL_optnumber(L, 2, 0);
		utb.modtime = (time_t)luaL_optinteger(L, 3, utb.actime);
		buf = &utb;
	}

	return push_result(L, utime(file, buf), NULL);
}

int _file_type(const char *path, const char **type)
{
	STAT_STRUCT info;
	if (STAT_FUNC(path, &info)) {
		return -1;
	} else {
		*type = mode2string(info.st_mode);
		return 0;
	}
}

int eli_file_type(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	const char *type;
	if (_file_type(path, &type)) {
		lua_pushnil(L);
		lua_pushfstring(L,
				"cannot obtain information from path '%s': %s",
				path, strerror(errno));
		lua_pushinteger(L, errno);
		return 3;
	}
	lua_pushstring(L, type);
	return 1;
}

int _link_type(const char *path, const char **type)
{
	STAT_STRUCT info;
	if (LSTAT_FUNC(path, &info)) {
		return -1;
	} else {
		*type = mode2string(info.st_mode);
		return 0;
	}
}

int eli_link_type(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	const char *type;
	if (_link_type(path, &type)) {
		lua_pushnil(L);
		lua_pushfstring(L,
				"cannot obtain information from path '%s': %s",
				path, strerror(errno));
		lua_pushinteger(L, errno);
		return 3;
	}
	lua_pushstring(L, type);
	return 1;
}