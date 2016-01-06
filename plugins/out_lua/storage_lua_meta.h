//
// Created by jc on 5/1/2016.
//

#include <fluent-bit/flb_config.h>
#include <stdint.h>
#include <stddef.h>

#ifndef FLUENT_BIT_STORAGE_LUA_META_H
#define FLUENT_BIT_STORAGE_LUA_META_H

struct lua_meta_data {
    char file_name[1024];
    size_t name_len;
    char file_dir[1024];
    size_t dir_len;
    uint64_t crc32_code;
    uint64_t offset;
};

struct meta_list {
    int meta_fd;
    struct mk_list entries;
};

struct meta_node {
    uint64_t key;
    struct lua_meta_data value;
    struct mk_list _head;
};

void lua_meta_init(struct meta_list *list);

void lua_meta_destory(struct meta_list *list);

struct meta_node *
lua_meta_get(struct meta_list *list, uint64_t key);

void lua_meta_del(struct meta_list *list, uint64_t key);

void lua_meta_debug_display(struct meta_list *list);

#endif //FLUENT_BIT_STORAGE_LUA_META_H
