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

/* STDIN Input configuration & context */
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
    struct flb_in_lua_file_info *next;
    struct flb_in_lua_file file_config;
    char file_name[1024];
    int file_fd;
    bool new_file;
};

struct flb_in_lua_exec_info {
    struct flb_in_lua_exec_info *next;
    struct flb_in_lua_exec exec_config;
};

struct flb_in_lua_stat_info {
    struct flb_in_lua_stat_info *next;
    struct flb_in_lua_stat stat_config;
};

enum config_key{
    config_file = 0,
    config_exec,
    config_stat,
    config_max
};

typedef void (*in_lua_config_layer_two)(lua_State *, struct mk_rconf *, char *);

struct flb_in_lua_callback{
    char *key;
    char *prefix;
    char *layer_prefix;
    in_lua_config_layer_two pfunc;
};

/*
#define SET_format(_conf, _format) {\
    _conf->format = _format; \
}

#define SET_call(_conf, _call) {\
    _conf->call = _call; \
}

#define SET_shell(_conf, _shell) {\
    _conf->shell = _shell; \
}

#define SET_watch(_conf, _watch) {\
    _conf->watch = _watch; \
}

#define SET_refresh_interval(_conf, _refresh_interval) { \
    _conf->refresh_interval = aton(_refresh_interval); \
}

#define SET_hostname(_conf, _hostname) { \
    _conf->hostname = _hostname; \
}

#define SET_joural_directory(_conf, _joural_directory) { \
    _conf->joural_directory = _joural_directory; \
}

#define SET_log_directory(_conf, _log_directory) { \
    _conf->log_directory = _log_directory; \
}

#define SET_file_match(_conf, _file_match) {\
    _conf->file_match = _file_match; \
}

#define SET_priority(_conf, _priority) {\
    _conf->priority = _priority;\
}

#define SET_rescan_interval(_conf, _rescan_interval) { \
    _conf->rescan_interval = _aton(rescan_interval); \
}

#define SET_CONF_Field(_conf, _key, _val) {\
    SET_##_key((_conf), (_val));\
}
*/
/*
void FILE_SET_hostname(struct flb_in_lua_file *conf, char * hostname)
{
    conf->hostname = hostname;
    return;
}
*/
int in_lua_init(struct flb_config *config);
int in_lua_collect(struct flb_config *config, void *in_context);
void *in_lua_flush(void *in_context, int *size);

extern struct flb_input_plugin in_lua_plugin;

#endif

