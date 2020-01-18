#include "lua.h"

int eli_pipe(lua_State *L);
int pipe_create_meta(lua_State *L);

typedef struct ELI_PIPE
{
#ifdef _WIN32
    HANDLE h;
#else
    int fd;
#endif
    int closed;
    int nonblocking;
    const char * mode;
} ELI_PIPE;

#define PIPE_METATABLE "ELI_PIPE"
