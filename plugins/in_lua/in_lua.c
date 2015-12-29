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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <msgpack.h>
#include <fluent-bit/flb_input.h>
#include <fluent-bit/flb_config.h>
#include <fluent-bit/flb_pack.h>
/* LUA Include */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "in_lua.h"

static int gai_pipe_fd_data[2] = {};
static int gai_pipe_fd_control[2] = {};


int pipe_write_data(lua_State *L)
{
    const char *pc_data = NULL;
    pc_data = luaL_checkstring(L, 1);
    write(gai_pipe_fd_data[1], pc_data, strlen(pc_data));
    return 0;
}


int pipe_read_control(lua_State *L)
{
    char sz_command[1024];
    int i_read_len = read(gai_pipe_fd_control[0], sz_command, sizeof(sz_command) - 1);
    if (i_read_len > 0)
    {
        sz_command[i_read_len] = '\0';
        lua_pushstring(L, sz_command);
    }
    return 1;
}

int pipe_write_control(const char *pc_command, const unsigned int ui_command_len)
{
    assert(NULL != pc_command);
    write(gai_pipe_fd_control[1], pc_command, ui_command_len);
    return 0;
}

int pipe_read_data(char *pc_buf, const unsigned int ui_buf_len)
{
    assert(NULL != pc_buf);
    int i_read_len = read(gai_pipe_fd_data[0], pc_buf, ui_buf_len - 1);
    if (i_read_len > 0)
    {
        pc_buf[i_read_len] = '\0';
    }
    return i_read_len;
}

void in_lua_get_data()
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_register(L, "write", pipe_write_data);
    lua_register(L, "read", pipe_read_control);
    sleep(1);
    luaL_dofile(L, "/home/ma/work/fluent-bit/test/test.lua");
}


int in_lua_get_pipe()
{
    printf("call in_lua_get_pipe\r\n");
    pid_t i_child_pid = -1;
    if (0 > pipe(gai_pipe_fd_data) || 0 > pipe(gai_pipe_fd_control))
    {
        printf("fluentd pipe create failed.\r\n");
        exit(0);
    }
    i_child_pid = fork();
    if (i_child_pid < 0)
    {
        printf("fluentd fork create failed.\r\n");
        exit(0);
    }
    else if (i_child_pid == 0)
    {
        close(gai_pipe_fd_data[0]);
        close(gai_pipe_fd_control[1]);

        gai_pipe_fd_control[1] = -1;
        gai_pipe_fd_data[0] = -1;
        
        in_lua_get_data();
        exit(0);
    }
    else
    {
        close(gai_pipe_fd_data[1]);
        close(gai_pipe_fd_control[0]);
        gai_pipe_fd_control[0] = -1;
        gai_pipe_fd_data[1] = -1;
        return gai_pipe_fd_data[0];
    }
    return 0;
}

void in_lua_file_conf(struct mk_rconf *conf, char *key)
{

    struct flb_in_lua_file file;
    struct mk_rconf_section *section;
    struct mk_rconf_entry *entry;
    struct mk_list *head;

    section = mk_rconf_section_get(conf, key);
    if (section)
    {
        mk_list_foreach(head, &section->entries)
        {
            entry = mk_list_entry(head, struct mk_rconf_entry, _head);
            printf("section key = %s, val = %s\n", entry->key, entry->val);
            if (0 == strcasecmp(entry->key, "hostname")) {
                //SET_CONF_Field(&file, hostname, entry->val);
                file.hostname = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "jounal_directory")) {
                //SET_CONF_Field(&file, jounal_directory, entry->val);
                file.joural_directory = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "log_directory")) {
                //SET_CONF_Field(&file, log_directory, entry->val);
                file.log_directory = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "file_match")) {
                //SET_CONF_Field(&file, file_match, entry->val);
                file.file_match = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "rescan_interval")) {
                //SET_CONF_Field(&file, rescan_interval, entry->val);
                file.rescan_interval = atoi(entry->val);
            }
            else if(0 == strcasecmp(entry->key, "priority")) {
                //SET_CONF_Field(&file, priority, entry->val);
                file.priority = entry->val;
            }
            else {
                printf("config [%s] not support %s", key, entry->key);
            }
        }
    }
}


void in_lua_config(struct flb_in_lua_config* ctx, struct mk_rconf *conf)
{
    /*
     * 从文件中加载 配置信息
     * */

    if (NULL == conf || NULL == ctx)
    {
        return;
    }
    struct mk_rconf_section *section;

    struct mk_rconf_entry *entry;
    struct mk_list *head;

    /* 初始化系统的环境变量 */
    {
        lua_State *L = (lua_State*)ctx->lua_state;
        section = mk_rconf_section_get(conf, "LS");
        if (section) {
            /* Validate TD section keys */
            if(MK_TRUE == (size_t)mk_rconf_section_get_key(section, "lua_debug", MK_RCONF_BOOL))
                luaopen_debug(L);
            if(MK_TRUE == (size_t)mk_rconf_section_get_key(section, "lua_package", MK_RCONF_BOOL))
                luaopen_package(L);
        }
        // set package path
        // load the package
    }
    section = mk_rconf_section_get(conf, "FILE");
    if (section)
    {
        mk_list_foreach(head, &section->entries)
        {
            entry = mk_list_entry(head, struct mk_rconf_entry, _head);
            printf("FILE key = %s, val = %s\n", entry->key, entry->val);

            if (0 == strcasecmp(entry->val, MK_RCONF_ON))
            {
                in_lua_file_conf(conf, entry->key);
            }
        }
    }

    section = mk_rconf_section_get(conf, "EXEC");
    if (section)
    {
        mk_list_foreach(head, &section->entries)
        {
            entry = mk_list_entry(head, struct mk_rconf_entry, _head);
            printf("FILE key = %s, val = %s\n", entry->key, entry->val);
        }
    }

    section = mk_rconf_section_get(conf, "STAT");
    if (section)
    {
        mk_list_foreach(head, &section->entries)
        {
            entry = mk_list_entry(head, struct mk_rconf_entry, _head);
            printf("FILE key = %s, val = %s\n", entry->key, entry->val);
        }
    }


    /*
    section = mk_rconf_section_get(conf, "File:nginx_access");
    if (section) {
        // Validate TD section keys
        //config->listen = mk_rconf_section_get_key(section, "Listen", MK_RCONF_STR);
        //config->tcp_port = mk_rconf_section_get_key(section, "Port", MK_RCONF_STR);
        // enum all keys
        //struct mk_rconf_entry *entry;
        //struct mk_list *head;
        mk_list_foreach(head, &section->entries){
            entry = mk_list_entry(head, struct mk_rconf_entry, _head);
            printf("section key= %s\n", entry->key);
        }
    }
*/
}

/* Cleanup lua input */
int in_lua_exit(void *in_context, struct flb_config *config)
{
    /* 必须关闭 LUA 虚拟机， 让 LUA 端 可以释放资源 */
    struct flb_in_lua_config *ctx = in_context;
    lua_State *L = ctx->lua_state;

    flb_debug("[in_lua] shutdown lua engine...");
    lua_close(L);
    return 0;
}

/* Initialize plugin */
int in_lua_init(struct flb_config *config)
{
    int fd;
    int ret;
    struct flb_in_lua_config *ctx;


    /* Allocate space for the configuration */
    ctx = malloc(sizeof(struct flb_in_lua_config));
    if (!ctx) {
        return -1;
    }
    /* 初始化 LUA 环境 */
    lua_State *L = luaL_newstate();
    ctx->lua_state = L;
    // using as sanbox
    luaopen_base(L);
    luaopen_table(L);
    //luaopen_io(L);
    //luaopen_os(L);
    luaopen_string(L);
    luaopen_math(L);
    // load _debug & _package in in_lua_config
    /* read the configure */
    in_lua_config(ctx, config->file);

    /* initialize MessagePack buffers */
    msgpack_sbuffer_init(&ctx->mp_sbuf);
    msgpack_packer_init(&ctx->mp_pck, &ctx->mp_sbuf, msgpack_sbuffer_write);
    ctx->buffer_id = 0;

    /* Clone the standard input file descriptor */
    //fd = dup(STDOUT_FILENO);
    fd = in_lua_get_pipe();
    if (fd == -1) {
        perror("dup");
        flb_utils_error_c("Could not open standard input!");
    }
    ctx->fd = fd;

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
    int bytes;
    int out_size;
    int ret;
    char *pack;
    msgpack_unpacked result;
    size_t start = 0, off = 0;
    struct flb_in_lua_config *ctx = in_context;

    bytes = read(ctx->fd,
                 ctx->buf + ctx->buf_len,
                 sizeof(ctx->buf) - ctx->buf_len);
    flb_debug("in_lua read() = %i", bytes);
    if (bytes <= 0) {
        return -1;
    }
    ctx->buf_len += bytes;

    /* Initially we should support JSON input */
    ret = flb_pack_json(ctx->buf, ctx->buf_len, &pack, &out_size);
    if (ret != 0) {
        flb_debug("lua data incomplete, waiting for more data...");
        return 0;
    }
    ctx->buf_len = 0;

    /* Queue the data with time field */
    msgpack_unpacked_init(&result);

    while (msgpack_unpack_next(&result, pack, out_size, &off)) {
        if (result.data.type == MSGPACK_OBJECT_MAP) {
            /* { map => val, map => val, map => val } */
            msgpack_pack_array(&ctx->mp_pck, 2);
            msgpack_pack_uint64(&ctx->mp_pck, time(NULL));
            msgpack_pack_bin_body(&ctx->mp_pck, pack + start, off - start);
        } else {
            msgpack_pack_bin_body(&ctx->mp_pck, pack + start, off - start);
        }
        ctx->buffer_id++;

        start = off;
    }
    msgpack_unpacked_destroy(&result);

    free(pack);
    return 0;
}

void *in_lua_flush(void *in_context, int *size)
{
    char *buf;
    msgpack_sbuffer *sbuf;
    struct flb_in_lua_config *ctx = in_context;

    if (ctx->buffer_id == 0)
        goto fail;

    sbuf = &ctx->mp_sbuf;
    *size = sbuf->size;
    buf = malloc(sbuf->size);
    if (!buf)
        goto fail;

    /* set a new buffer and re-initialize our MessagePack context */
    memcpy(buf, sbuf->data, sbuf->size);
    msgpack_sbuffer_destroy(&ctx->mp_sbuf);
    msgpack_sbuffer_init(&ctx->mp_sbuf);
    msgpack_packer_init(&ctx->mp_pck, &ctx->mp_sbuf, msgpack_sbuffer_write);

    ctx->buffer_id = 0;

    return buf;

fail:
    return NULL;
}

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

