#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lerror.h"
#include "lfsutil.h"

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
#define STAT_STRUCT struct _stat
#define STAT_FUNC _stat

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

#define _lmkdir(path)                                                    \
	(mkdir((path), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | \
			       S_IXGRP | S_IROTH | S_IXOTH))

#endif

typedef struct dir_data {
	int closed;
	char *path;
#ifdef _WIN32
	intptr_t hFile;
	char pattern[LMAXPATHLEN + 1];
#else
	DIR *dir;
#endif
	int as_dir_entries;
} dir_data;

typedef struct dir_entry_data {
	char *folder;
	char *name;
	int closed;
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
	return name[0] == '.' &&
	       (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

int eli_read_dir(lua_State *L)
{
#ifdef _WIN32
	struct _finddata_t c_file;
#else
	struct dirent *entry;
#endif
	const char *path = luaL_checkstring(L, 1);
	const int as_dir_entries = lua_toboolean(L, 2);
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
	if ((hFile = _findfirst(pattern, &c_file)) == -1L) {
		return 1;
	} else if (!isdotfile(c_file.name)) {
		if (as_dir_entries) {
			struct dir_entry_data *result = lua_newuserdata(
				L, sizeof(struct dir_entry_data));
			result->name = clone_string(c_file.name);
			result->folder = clone_string(path);
			result->closed = 0;
			luaL_getmetatable(L, DIR_ENTRY_METATABLE);
			lua_setmetatable(L, -2);
			lua_rawseti(L, resultPosition, i++); /* t[i] = result */
		} else {
			lua_pushstring(L, c_file.name); /* push path */
			lua_rawseti(L, resultPosition, i++); /* t[i] = result */
		}
	}

	while (_findnext(hFile, &c_file) != -1L) {
		if (isdotfile(c_file.name))
			continue;
		if (as_dir_entries) {
			struct dir_entry_data *result = lua_newuserdata(
				L, sizeof(struct dir_entry_data));
			result->name = clone_string(c_file.name);
			result->folder = clone_string(path);
			result->closed = 0;
			luaL_getmetatable(L, DIR_ENTRY_METATABLE);
			lua_setmetatable(L, -2);
			lua_rawseti(L, resultPosition, i++); /* t[i] = result */
		} else {
			lua_pushstring(L, c_file.name); /* push path */
			lua_rawseti(L, resultPosition, i++); /* t[i] = result */
		}
	}
	_findclose(hFile);
#else
	DIR *dir = opendir(path);
	if (dir == NULL) {
		char error_msg[1024];
		snprintf(error_msg, sizeof(error_msg), "cannot open %s: %s",
			 path, strerror(errno));
		return push_error(L, error_msg);
	}

	int i = 1;
	while ((entry = readdir(dir)) != NULL) {
		if (isdotfile(entry->d_name))
			continue;
		if (as_dir_entries) {
			struct dir_entry_data *result = lua_newuserdata(
				L, sizeof(struct dir_entry_data));
			result->name = clone_string(entry->d_name);
			result->folder = clone_string(path);
			result->closed = 0;
			luaL_getmetatable(L, DIR_ENTRY_METATABLE);
			lua_setmetatable(L, -2);
			lua_rawseti(L, resultPosition, i++);
		} else {
			lua_pushstring(L, entry->d_name); /* push path */
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
	luaL_argcheck(L, d->closed == 0, 1,
		      "can not iterate over closed " DIR_METATABLE);
	const int as_dir_entries = d->as_dir_entries == -1 ?
					   lua_toboolean(L, 2) :
					   d->as_dir_entries;
#ifdef _WIN32
	if (d->hFile == 0L) { /* first entry */
		if ((d->hFile = _findfirst(d->pattern, &c_file)) == -1L) {
			lua_pushnil(L);
			lua_pushstring(L, strerror(errno));
			d->closed = 1;
			return 2;
		} else if (!isdotfile(c_file.name)) {
			if (as_dir_entries) {
				struct dir_entry_data *result = lua_newuserdata(
					L, sizeof(struct dir_entry_data));
				result->name = clone_string(c_file.name);
				result->folder = clone_string(d->path);
				result->closed = 0;
				luaL_getmetatable(L, DIR_ENTRY_METATABLE);
				lua_setmetatable(L, -2);
			} else {
				lua_pushstring(L, c_file.name);
			}
			return 1;
		}
	}
	// skip dot files
	long found;
	while ((found = _findnext(d->hFile, &c_file)) != -1L &&
	       isdotfile(c_file.name))
		continue;

	/* next entry */
	if (found == -1L) {
		/* no more entries => close directory */
		_findclose(d->hFile);
		d->closed = 1;
		return 0;
	} else {
		if (as_dir_entries) {
			struct dir_entry_data *result = lua_newuserdata(
				L, sizeof(struct dir_entry_data));
			result->name = clone_string(c_file.name);
			result->folder = clone_string(d->path);
			result->closed = 0;
			luaL_getmetatable(L, DIR_ENTRY_METATABLE);
			lua_setmetatable(L, -2);
		} else {
			lua_pushstring(L, c_file.name);
		}
		return 1;
	}

#else
	// skip dot files
	while ((entry = readdir(d->dir)) != NULL && isdotfile(entry->d_name))
		continue;

	if (entry != NULL) {
		if (as_dir_entries) {
			struct dir_entry_data *result = lua_newuserdata(
				L, sizeof(struct dir_entry_data));
			result->name = clone_string(entry->d_name);
			result->folder = clone_string(d->path);
			result->closed = 0;
			luaL_getmetatable(L, DIR_ENTRY_METATABLE);
			lua_setmetatable(L, -2);
		} else {
			lua_pushstring(L, entry->d_name);
		}
		return 1;
	} else {
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
	d->path = clone_string(path);
	d->as_dir_entries = -1;
#ifdef _WIN32
	d->hFile = 0L;
	if (strlen(path) > LMAXPATHLEN - 2)
		return luaL_error(L, "path too long: %s", path);
	else
		sprintf(d->pattern, "%s/*", path);
#else
	d->dir = opendir(path);
	if (d->dir == NULL) {
		char error_msg[1024];
		snprintf(error_msg, sizeof(error_msg), "cannot open %s: %s",
			 path, strerror(errno));
		return push_error(L, error_msg);
	}
#endif
	return 1;
}

int eli_iter_dir(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	int as_dir_entries = lua_toboolean(L, 2);
	dir_data *d;

	lua_pushcfunction(L, dir_iter);
	d = (dir_data *)lua_newuserdata(L, sizeof(dir_data));
	luaL_getmetatable(L, DIR_METATABLE);
	lua_setmetatable(L, -2);
	d->closed = 0;
	d->as_dir_entries = as_dir_entries;
	d->path = clone_string(path);
#ifdef _WIN32
	d->hFile = 0L;
	if (strlen(path) > LMAXPATHLEN - 2)
		return luaL_error(L, "path too long: %s", path);
	else
		sprintf(d->pattern, "%s/*", path);
#else
	d->dir = opendir(path);
	if (d->dir == NULL) {
		char error_msg[1024];
		snprintf(error_msg, sizeof(error_msg), "cannot open %s: %s",
			 path, strerror(errno));
		return push_error(L, error_msg);
	}
#endif
	lua_pushnil(L);
	lua_pushvalue(L, -2);
	return 4;
}

/*
** Closes directory iterators
*/
static int lclosedir(lua_State *L)
{
	dir_data *d = (dir_data *)luaL_checkudata(L, 1, DIR_METATABLE);
#ifdef _WIN32
	if (!d->closed && d->hFile) {
		_findclose(d->hFile);
		free(d->path);
	}
#else
	if (!d->closed && d->dir) {
		closedir(d->dir);
		free(d->path);
	}
#endif
	d->closed = 1;
	return 0;
}

static int dir_path(lua_State *L)
{
	dir_data *d = (dir_data *)luaL_checkudata(L, 1, DIR_METATABLE);
	luaL_argcheck(L, d->closed == 0, 1, "closed " DIR_METATABLE);
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
	lua_pushcfunction(L, lclosedir);
	lua_setfield(L, -2, "__close");
	return 1;
}

int dir_entry_type(lua_State *L)
{
	struct dir_entry_data *ded = (struct dir_entry_data *)luaL_checkudata(
		L, 1, DIR_ENTRY_METATABLE);
	luaL_argcheck(L, ded->closed == 0, 1, "closed " DIR_ENTRY_METATABLE);
	char *path = joinpath(ded->folder, ded->name);

	if (!path) {
		push_error(L, "Out of memory");
	}

	STAT_STRUCT info;
	if (STAT_FUNC(path, &info)) {
		lua_pushnil(L);
		lua_pushfstring(L,
				"cannot obtain information from path '%s': %s",
				path, strerror(errno));
		lua_pushinteger(L, errno);
		return 3;
	}
	free(path);
	lua_pushstring(L, mode2string(info.st_mode));
	return 1;
}

static int dir_entry_name(lua_State *L)
{
	struct dir_entry_data *ded = (struct dir_entry_data *)luaL_checkudata(
		L, 1, DIR_ENTRY_METATABLE);
	luaL_argcheck(L, ded->closed == 0, 1, "closed " DIR_ENTRY_METATABLE);

	lua_pushstring(L, ded->name);
	return 1;
}

static int dir_entry_fullpath(lua_State *L)
{
	struct dir_entry_data *ded = (struct dir_entry_data *)luaL_checkudata(
		L, 1, DIR_ENTRY_METATABLE);
	luaL_argcheck(L, ded->closed == 0, 1, "closed " DIR_ENTRY_METATABLE);

	char *res = joinpath(ded->folder, ded->name);
	if (!res) {
		push_error(L, "Out of memory");
	}

	lua_pushstring(L, res);
	free(res);
	return 1;
}

static int dir_entry_close(lua_State *L)
{
	dir_entry_data *d =
		(dir_entry_data *)luaL_checkudata(L, 1, DIR_ENTRY_METATABLE);
	if (!d->closed) {
		free(d->name);
		free(d->folder);
	}
	d->closed = 1;
	return 0;
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
	lua_pushcfunction(L, dir_entry_close);
	lua_setfield(L, -2, "__gc");
	lua_pushcfunction(L, dir_entry_close);
	lua_setfield(L, -2, "__close");
	return 1;
}
