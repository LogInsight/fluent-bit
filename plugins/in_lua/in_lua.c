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
#include <lua5.1/lua.h>
#include <lua5.1/lauxlib.h>
#include <lua5.1/lualib.h>

#include "in_lua.h"
static int gai_pipe_fd_data[2] = {};
static int gai_pipe_fd_control[2] = {};


void in_lua_file_conf(struct mk_rconf *conf, char *key);
void in_lua_exec_conf(struct mk_rconf *conf, char *key);
void in_lua_stat_conf(struct mk_rconf *conf, char *key);

static struct flb_in_lua_callback gst_config_call[config_max] = {
    [config_file] = {"FILE", "file_", "File:", in_lua_file_conf},
    [config_exec] = {"EXEC", "exec_", "Exec:", in_lua_exec_conf},
    [config_stat] = {"STAT", "stat_", "Stat:", in_lua_stat_conf}
};

static struct flb_in_lua_global gst_global_config;

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

            if(0 == strcasecmp(entry->key, "journal_directory")) {
                file.journal_directory = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "log_directory")) {
                file.log_directory = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "file_match")) {
                file.file_match = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "rescan_interval")) {
                file.rescan_interval = atoi(entry->val);
            }
            else if(0 == strcasecmp(entry->key, "priority")) {
                file.priority = entry->val;
            }
            else {
                printf("config [%s] not support %s.\r\n", key, entry->key);
            }
        }
    }
}

void in_lua_exec_conf(struct mk_rconf *conf, char *key)
{

    struct flb_in_lua_exec file;
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
            if(0 == strcasecmp(entry->key, "watch")) {
                file.watch = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "shell")) {
                file.shell = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "call")) {
                file.call = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "refresh_interval")) {
                file.refresh_interval = atoi(entry->val);
            }
            else {
                printf("config [%s] not support %s.\r\n", key, entry->key);
            }
        }
    }
}

void in_lua_stat_conf(struct mk_rconf *conf, char *key)
{

    struct flb_in_lua_stat file;
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

            if(0 == strcasecmp(entry->key, "format")) {
                file.format = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "refresh_interval")) {
                file.refresh_interval = atoi(entry->val);
            }
            else {
                printf("config [%s] not support %s.\r\n", key, entry->key);
            }
        }
    }
}

void in_lua_global_config(struct mk_rconf *conf)
{
    struct mk_rconf_section *section;
    struct mk_rconf_entry *entry;
    struct mk_list *head;

    memset(&gst_global_config, 0, sizeof(gst_global_config);

    section = mk_rconf_section_get(conf, "LS");
    if (section)
    {
        mk_list_foreach(head, &section->entries)
        {
            entry = mk_list_entry(head, struct mk_rconf_entry, _head);
            printf("section key = %s, val = %s\n", entry->key, entry->val);

            if(0 == strcasecmp(entry->key, "hostname")) {
                gst_global_config.hostname = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "refresh_interval")) {
                gst_global_config.refresh_interval = atoi(entry->val);
            }
            else {
                printf("config [LS] not support %s.\r\n", entry->key);
            }
        }
    }
}

void in_lua_config(struct mk_rconf *conf)
{
    if (NULL == conf)
    {
        return;
    }

    int loop_num = 0;
    struct mk_rconf_section *section;
    struct mk_rconf_entry *entry;
    struct mk_list *head;

    in_lua_global_config(conf);

    for (loop_num = 0; loop_num < config_max; loop_num ++)
    {
        section = mk_rconf_section_get(conf, gst_config_call[loop_num].key);
        if (section)
        {
            mk_list_foreach(head, &section->entries)
            {
                entry = mk_list_entry(head, struct mk_rconf_entry, _head);
                printf("FILE key = %s, val = %s\n", entry->key, entry->val);

                if (0 == strcasecmp(entry->val, MK_RCONF_ON))
                {
                    if (0 == strncasecmp(entry->key,
                                         gst_config_call[loop_num].prefix,
                                         strlen(gst_config_call[loop_num].prefix))) {
                        memcpy(entry->key,
                               gst_config_call[loop_num].layer_prefix,
                               strlen(gst_config_call[loop_num].prefix));

                        gst_config_call[loop_num].pfunc(conf, entry->key);
                    }
                    else {
                        printf("config prefix of %s in [%s] is wrong. should be \"file_\"\r\n",
                               entry->key,
                               gst_config_call[loop_num].key);
                    }
                }
            }
        }
    }
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
    /* read the configure */
    in_lua_config(config->file);

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
    .cb_flush_buf = in_lua_flush
};
