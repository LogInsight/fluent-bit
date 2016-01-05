//
// Created by jc on 5/1/2016.
//

#include "storage_lua.h"
#include <msgpack.h>
#include <stdio.h>
#include "storage_command.h"
#include "storage_lua_meta.h"

#include <fluent-bit/flb_utils.h>
#include <fluent-bit/flb_io.h>

static const size_t send_buf_size = 1024 * 1024;

struct flb_output_plugin out_lua_plugin;

int cb_lua_init(struct flb_output_plugin *plugin, struct flb_config *config)
{
    flb_info("call out_lua_init");
    int ret;
    struct flb_out_lua_config *ctx;
    struct flb_io_upstream *upstream;

    ctx = calloc(1, sizeof(struct flb_out_lua_config));
    if (!ctx) {
        perror("calloc");
        return -1;
    }

    /* Set default network configuration */
    if (!plugin->net_host) {
        plugin->net_host = strdup("192.168.10.175");
    }
    if (plugin->net_port == 0) {
        plugin->net_port = 11110;
    }

    /* Prepare an upstream handler */
    upstream = flb_io_upstream_new(config,
                                   plugin->net_host,
                                   plugin->net_port,
                                   FLB_IO_TCP, NULL);
    if (!upstream) {
        free(ctx);
        return -1;
    }
    ctx->stream = upstream;

    ret = flb_output_set_context("out_lua", ctx, config);
    if (ret == -1) {
        flb_utils_error_c("Could not set configuration for lua output plugin");
    }

    lua_meta_init(&ctx->m_list);
    ctx->buf = malloc(1024 * 1024);

    struct command_connect_req_head req_head;
    memset (&req_head, 0 , sizeof(req_head));
    req_head.userid = 10;
    req_head.host = 0;
    req_head.version = 1;
    req_head.tlv_len = 0;

    size_t buf_len = send_buf_size - 4;
    size_t req_len = 0;
    pack_command_connect(&req_head, ctx->buf + 4, buf_len, &req_len);
    uint32_t pack_len = req_len;
    pack_len = ntohl(pack_len);
    memcpy(ctx->buf, (const void *)&pack_len, sizeof(uint32_t));
    ctx->buf_len = req_len + 4;

    size_t out_len = 0;

    flb_io_net_write(ctx->stream, ctx->buf, ctx->buf_len, &out_len);
    return 0;
}

int cb_lua_exit(void *data, struct flb_config *config)
{
    (void) config;
    struct flb_out_lua_config *ctx = data;
    lua_meta_destory(&ctx->m_list);
    free(ctx);

    return 0;
}

int cb_lua_flush(void *data, size_t bytes, void *out_context,
                     struct flb_config *config)
{
    flb_info("call out lua flush");
    int ret = -1;
    int entries = 0;
    size_t off = 0;
    size_t total;
    size_t bytes_sent;
    char *buf = NULL;
    msgpack_packer   mp_pck;
    msgpack_sbuffer  mp_sbuf;
    msgpack_unpacked result;
    struct flb_out_lua_config *ctx = out_context;
    (void) config;
    printf("the flush data = %.*s\n", (int) bytes, (char *)data);

    return ret;
}

/* Plugin reference */
struct flb_output_plugin out_lua_plugin = {
        .name         = "out_lua",
        .description  = "lua data collector",
        .cb_init      = cb_lua_init,
        .cb_pre_run   = NULL,
        .cb_flush     = cb_lua_flush,
        .cb_exit      = cb_lua_exit,
        .flags        = FLB_OUTPUT_NET,
};
