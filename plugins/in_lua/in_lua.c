/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2015 Treasure Data Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#include <msgpack.h>
#include <fluent-bit/flb_input.h>

/* LUA Include */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "in_lua.h"
#include "in_lua_config.h"
#include "in_lua_file.h"

/* Cleanup lua input */
int in_lua_exit(void *in_context, struct flb_config *config)
{
    /* 必须关闭 LUA 虚拟机， 让 LUA 端 可以释放资源 */
    struct flb_in_lua_config *ctx = in_context;
    if(ctx->lua_state) {
        lua_State *L = ctx->lua_state;

        flb_debug("[in_lua] shutdown lua engine...");
        lua_close(L);
        ctx->lua_state = NULL;
    }
    /* free config->index_files */
    if (ctx->lua_paths)
        mk_string_split_free(ctx->lua_paths);
    free(ctx->buf);
    ctx->buf = NULL;
    return 0;
}

static void in_lua_open_libs(lua_State *L) {
    /* 初始化 LUA 环境 */
    // using as sanbox
    //luaopen_base(L);
    lua_pushcfunction(L, luaopen_base);
    lua_pushstring(L, "");
    lua_call(L, 1, 0);

    //luaopen_table(L);
    flb_info("b luaopen_table");
    lua_pushcfunction(L, luaopen_table);
    lua_pushstring(L, "");
    lua_call(L, 1, 0);

    //luaopen_string(L);
    lua_pushcfunction(L, luaopen_string);
    lua_pushstring(L, "");
    lua_call(L, 1, 0);

    //luaopen_math(L);
    lua_pushcfunction(L, luaopen_math);
    lua_pushstring(L, "");
    lua_call(L, 1, 0);

    return;
}
static void in_lua_ctx_init(struct flb_in_lua_config *ctx) {
    flb_info("in_lua_ctx_init");
    memset(ctx, 0, sizeof(struct flb_in_lua_config));
    ctx->fd = -1;
    ctx->lua_paths = NULL;
    mk_list_init(&ctx->file_config);
    mk_list_init(&ctx->exec_config);
    mk_list_init(&ctx->stat_config);

    lua_State *L = luaL_newstate();

    if (NULL == L) {
        flb_utils_error_c("lua stack open error.");
    }

    in_lua_open_libs(L);
    ctx->lua_state = L;
    return;
}

/* Initialize plugin */
int in_lua_init(struct flb_config *config)
{
    int ret;
    struct flb_in_lua_config *ctx;

    struct mk_event event;

    flb_info("call in_lua_init");


    /* Allocate space for the configuration */
    ctx = malloc(sizeof(struct flb_in_lua_config));
    if (!ctx) {
        flb_error("malloc failed.");
        return -1;
    }

    in_lua_ctx_init(ctx);

    ctx->evl = config->evl;


    in_lua_config(ctx, config->file);

    in_lua_file_init(ctx);

    event.mask = MK_EVENT_EMPTY;
    event.status = MK_EVENT_NONE;

    ctx->fd = mk_event_timeout_create(ctx->evl, 1, &event);
    if (ctx->fd == -1) {
        flb_utils_error_c("Could not open standard input!");
    }

    /* Set the context */
    ret = flb_input_set_context("lua", ctx, config);
    if (ret == -1) {
        flb_utils_error_c("Could not set configuration for lua input plugin");
    }


    /* Collect upon data available on the standard input */
    ret = flb_input_set_collector_event("lua",
                                        in_lua_collect,
                                        ctx->fd,
                                        config);
    if (ret == -1) {
        flb_utils_error_c("Could not set collector for lua input plugin");
    }

    return 0;
}

int in_lua_collect(struct flb_config *config, void *in_context)
{
    static uint64_t all_time_record = 0;

    //char *pack;
    //msgpack_unpacked result;
    struct flb_in_lua_config *ctx = in_context;
    struct mk_list *head;

    struct flb_in_lua_exec_info *exec;

    if (0 == all_time_record % IN_LUA_DEFAULT_RESAN_TIME) {
        in_lua_file_rescan(ctx);
    }

    mk_list_foreach(head, &ctx->exec_config) {
        exec = mk_list_entry(head, struct flb_in_lua_exec_info, _head);
        if (0 == all_time_record % exec->exec_config.refresh_interval) {
            //todo exec event
        }
    }


    return 0;
}

void *in_lua_flush(void *in_context, int *size)
{
    char *buf;
    struct flb_in_lua_config *ctx = in_context;

    buf = malloc(ctx->read_len);
    if (!buf)
        return NULL;

    *size = ctx->read_len;

    memcpy(buf, ctx->buf, ctx->read_len);
    return buf;
}
/*
int in_lua_pre_run(void *in_context, struct flb_config *config) {
    struct flb_in_lua_config *ctx = in_context;
    return 0;
}
*/
/* Plugin reference */
struct flb_input_plugin in_lua_plugin = {
    .name         = "lua",
    .description  = "Lua Input",
    .cb_init      = in_lua_init,
    .cb_pre_run   = NULL,
    .cb_collect   = in_lua_collect,
    .cb_flush_buf = in_lua_flush,
    .cb_exit      = in_lua_exit
};

