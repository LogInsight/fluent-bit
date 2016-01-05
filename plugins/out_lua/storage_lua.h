//
// Created by jc on 5/1/2016.
//

#ifndef FLUENT_BIT_STORAGE_LUA_H
#define FLUENT_BIT_STORAGE_LUA_H

#include "storage_lua_meta.h"

struct flb_out_lua_config {
    struct flb_io_upstream *stream;
    struct meta_list m_list;
    char *buf;
    size_t buf_len;
};

#endif //FLUENT_BIT_STORAGE_LUA_H
