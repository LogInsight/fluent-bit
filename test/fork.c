//
// Created by mayabin on 15/12/4.
//


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <lua5.1/lua.h>
#include <lua5.1/lauxlib.h>
#include <lua5.1/lualib.h>

int g_aParentPipeOutFd[2] = {};
int g_aParentPipeInFd[2] = {};


int fork_child(lua_State *L)
{
    pid_t iChildPID = -1;
    char szPipeData[1024];
    char *pSendData = "{\"abc\":\"efg\"}\r\n";
    int iDataLen = strlen(pSendData);
    int iLoop = 0;
    int iRead = 0;
    szPipeData[0] = '\0';

    if (0  > pipe(g_aParentPipeInFd) || 0 > pipe(g_aParentPipeOutFd))
    {
        printf("Create pipe line failed.\r\n");
        exit(0);
    }

    iChildPID = fork();
    if (iChildPID < 0)
    {
        printf("Create child process failed.\r\n");
        exit(0);
    }
    else if (iChildPID == 0)
    {
        close(g_aParentPipeInFd[0]);
        close(g_aParentPipeOutFd[1]);
        for(iLoop = 0; iLoop < 10; iLoop ++)
        {
            write(g_aParentPipeInFd[1], pSendData, iDataLen);
        }
        iRead = read(g_aParentPipeOutFd[0], szPipeData, 1024);
        write(STDOUT_FILENO, szPipeData, iRead);
        exit(0);
    }
    else
    {
        close(g_aParentPipeInFd[1]);
        close(g_aParentPipeOutFd[0]);
    }
    return 0;
}

int pipe_read(lua_State *L)
{
    char szPipeData[1024];
    szPipeData[0] = '\0';
    int iRead = read(g_aParentPipeInFd[0], szPipeData, 1024);
    
    szPipeData[iRead] = '\0';
    lua_pushstring(L, szPipeData);
    return 1;
}

int pipe_write(lua_State *L)
{
    const char *pcData = luaL_checkstring(L, 1);
    printf("%s\r\n", pcData);
    write(g_aParentPipeOutFd[1],pcData, strlen(pcData));
    return 0;
}

int stdout_write(lua_State *L)
{
    const char *pcData = NULL;
    pcData = luaL_checkstring(L, 1);
    write(STDOUT_FILENO, pcData, strlen(pcData));
    return 0;
}

static luaL_Reg mylibs[] = {
    {"fork", fork_child},
    {"read", pipe_read},
    {"write", pipe_write},
    {"write1", stdout_write},
    {NULL, NULL}
};


int luaopen_fork(lua_State *L)
{
    luaL_register(L, "fork", mylibs);
    return 1;
}

