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


/* LUA Include */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <fluent-bit/flb_config.h>
#include <fluent-bit/flb_input.h>
#include <fluent-bit/flb_utils.h>

/* LUA Input configuration & context */
struct flb_in_lua_config {
    //TODO: fork & pipe ?
    int fd;                           /* stdin file descriptor */
    int buf_len;                      /* read buffer length    */
    char buf[1 << 20];               /* read buffer: 1MB max */

    int buffer_id;
    struct msgpack_sbuffer mp_sbuf;  /* msgpack sbuffer             */
    struct msgpack_packer mp_pck;    /* msgpack packer              */
    void* lua_state;                 /* lua 的执行上下文              */
    char* lua_engine;
    struct mk_list * lua_paths;  /* lua 加载用户脚本使用的路径     */
    struct mk_list *file_config;
    struct mk_list *exec_config;
    struct mk_list *stat_config;
};

struct flb_in_lua_file{
    char *journal_directory;
    char *log_directory;
    char *file_match;
    char *priority;
    int rescan_interval;
};

struct flb_in_lua_exec{
    int refresh_interval;
    char *watch;
    char *shell;
    char *call;
};

struct flb_in_lua_stat{
    int refresh_interval;
    char *format;
};

struct flb_in_lua_global{
    int refresh_interval;
    char *hostname;
};

struct flb_in_lua_file_info {
    struct mk_list _head;
    struct flb_in_lua_file file_config;
    char file_name[1024];
    int file_fd;
    bool new_file;
};

struct flb_in_lua_exec_info {
    struct mk_list _head;
    struct flb_in_lua_exec exec_config;
};

struct flb_in_lua_stat_info {
    struct mk_list _head;
    struct flb_in_lua_stat stat_config;
};

enum config_key{
    config_file = 0,
    config_exec,
    config_stat,
    config_max
};

typedef void (*in_lua_config_layer_two)(struct flb_in_lua_config *, struct mk_rconf *, char *);

struct flb_in_lua_callback{
    char *key;
    char *prefix;
    char *layer_prefix;
    in_lua_config_layer_two pfunc;
};

int in_lua_init(struct flb_config *config);
int in_lua_collect(struct flb_config *config, void *in_context);
void *in_lua_flush(void *in_context, int *size);

extern struct flb_input_plugin in_lua_plugin;

#endif

