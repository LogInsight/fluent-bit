//
// Created by user on 4/1/2016.
//

#ifndef FLUENT_BIT_IN_LUA_TOOL_H
#define FLUENT_BIT_IN_LUA_TOOL_H

# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

unsigned long crc32_compute_buf(unsigned long inCrc32, const void *buf, size_t bufLen);

typedef uint32_t (*crc32_func)(uint32_t crc, const char *buf, unsigned int len);

extern crc32_func crc32_calc;

void crc32_init();


uint32_t
        crc32(const void *buf, size_t size);

int get_last_file(char* dir, uint32_t dir_size, char* filename, uint32_t file_size);

#endif //FLUENT_BIT_IN_LUA_TOOL_H
