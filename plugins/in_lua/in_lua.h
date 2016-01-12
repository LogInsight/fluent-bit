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

#ifndef FLB_IN_LUA_H
#define FLB_IN_LUA_H



#include <fluent-bit/flb_config.h>
#include <fluent-bit/flb_input.h>
#include <fluent-bit/flb_utils.h>
#include <mk_core/mk_event.h>
#include <stdbool.h>

#define IN_LUA_DEFAULT_RESAN_TIME   60

extern uint32_t g_stream_id;

/* LUA Input configuration & context */
struct flb_in_lua_config {
    //TODO: fork & pipe ?                 /* 如果是timer 模式，操作是打开，关闭，如果不是，则是常打开 */
    int fd;                           /* lua file descriptor */
    int buf_len;                      /* read buffer length    */
    char *buf;                        /* read buffer: 1MB */
    int read_len;
    char *read_buf;
    //struct msgpack_sbuffer mp_sbuf;  /* msgpack sbuffer             */
    //struct msgpack_packer mp_pck;    /* msgpack packer              */
    void* lua_state;                 /* lua 的执行上下文              */
    char* lua_engine;
    struct mk_list *lua_paths;  /* lua 加载用户脚本使用的路径     */
    struct mk_list file_config;
    struct mk_list exec_config;
    struct mk_list stat_config;
    struct mk_event_loop *evl;
    bool timer_mode;
};


int in_lua_data_write(void *buf, uint32_t len);
int in_lua_init(struct flb_config *config);
int in_lua_collect(struct flb_config *config, void *in_context);
void *in_lua_flush(void *in_context, int *size);

extern struct flb_input_plugin in_lua_plugin;

#endif

