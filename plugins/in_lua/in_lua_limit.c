//
// Created by mayabin on 16/1/2.
//

#include "in_lua_limit.h"

//
// Created by mayabin on 15/12/25.
//

#include <sys/time.h>
#include <unistd.h>

#include "limit.h"

#define TIME_INTERVAL              1000000
#define CPU_THRESHOLD              10
#define IO_THRESHOLD               20

static unsigned int g_uiCPUThreshold = CPU_THRESHOLD;
static unsigned int g_uiCPUTimeLimit = TIME_INTERVAL / 250 * CPU_THRESHOLD;
static unsigned int g_uiIOThreshold = IO_THRESHOLD;
static unsigned int g_uiIOBytesLimit = IO_THRESHOLD << 20;
static unsigned int g_uiStartTime = 0;
static unsigned int g_uiEndTime = 0;
static unsigned int g_uiCPUTimeUsed = 0;
static unsigned int g_uiIOBytesUsed = 0;

inline unsigned int LIMIT_GetCurrentTime()
{
    struct timeval stNow;
    gettimeofday(&stNow, NULL);
    return stNow.tv_sec * 1000000 + stNow.tv_usec;
}


inline void LIMIT_Reset()
{
    g_uiCPUTimeUsed = 0;
    g_uiIOBytesUsed = 0;
    g_uiStartTime = LIMIT_GetCurrentTime();
    g_uiEndTime = g_uiStartTime + TIME_INTERVAL;
}

void LIMIT_SetIOThreshold(unsigned int uiIOLimit)
{
    g_uiIOThreshold = uiIOLimit;
    g_uiIOBytesLimit = uiIOLimit << 20;
    g_uiIOBytesUsed = 0;
    return;
}

void LIMIT_SetCPUThreshold(unsigned int uiCPULimit)
{
    g_uiCPUThreshold = uiCPULimit;
    g_uiCPUTimeLimit = TIME_INTERVAL / 100 * uiCPULimit;
    g_uiCPUTimeUsed = 0;
    return;
}


inline void LIMIT_CheckCPULimit(unsigned int uiStartTime, unsigned int uiEndTime)
{
    if (uiEndTime >= g_uiEndTime)
    {
        LIMIT_Reset();
        return;
    }

    g_uiCPUTimeUsed += uiEndTime - uiStartTime;
    if (g_uiCPUTimeUsed >= g_uiCPUTimeLimit)
    {
        usleep(TIME_INTERVAL - g_uiCPUTimeUsed);
        LIMIT_Reset();
    }
    return;
}

inline void LIMIT_CheckIOLimit()
{
    unsigned int uiCurrentTime;
    if (g_uiIOBytesUsed >= g_uiIOBytesLimit)
    {
        uiCurrentTime = LIMIT_GetCurrentTime();
        if (uiCurrentTime >= g_uiEndTime)
        {
            LIMIT_Reset();
        }
        else
        {
            usleep(g_uiEndTime - uiCurrentTime);
            LIMIT_Reset();
        }
    }
    return;
}

inline void LIMIT_RecordIOInUsed(unsigned int uiBytes)
{
    g_uiIOBytesUsed += uiBytes;
    return;
}
