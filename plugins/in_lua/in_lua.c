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

