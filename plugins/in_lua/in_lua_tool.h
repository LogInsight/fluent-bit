//
// Created by user on 4/1/2016.
//

#ifndef FLUENT_BIT_IN_LUA_TOOL_H
#define FLUENT_BIT_IN_LUA_TOOL_H

# include <stdio.h>
# include <stdlib.h>

unsigned long Crc32_ComputeBuf(unsigned long inCrc32, const void *buf, size_t bufLen);

int Get_Last_File(char* dir, uint32_t dir_size, char* filename, uint32_t file_size);

#endif //FLUENT_BIT_IN_LUA_TOOL_H
