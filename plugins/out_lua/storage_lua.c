//
// Created by jc on 5/1/2016.
//

#include "storage_lua.h"
#include <msgpack.h>
#include <stdio.h>
#include "storage_pack.h"
#include "storage_command.h"
#include "storage_lua_meta.h"

#include <fluent-bit/flb_utils.h>
#include <fluent-bit/flb_io.h>
#include <fluent-bit/flb_lua_common.h>

const size_t send_buf_size = 1024 * 1024;

struct flb_output_plugin out_lua_plugin;

#define SERVPORT 12800
#define SERVER_IP "0"

int cb_lua_init(struct flb_output_plugin *plugin, struct flb_config *config) {
    flb_info("call out_lua_init");
    int ret;
    struct flb_out_lua_config *ctx;
    struct flb_io_upstream *upstream;

    ctx = calloc(1, sizeof(struct flb_out_lua_config));
    if (!ctx) {
        perror("calloc");
        return -1;
    }

    struct flb_in_lua_global *in_lua_config = in_lua_get_global();
    /* Set default network configuration */
    if (!plugin->net_host) {
        if (in_lua_config->server_ip == NULL)
            plugin->net_host = strdup(SERVER_IP);
        else
            plugin->net_host = strdup(in_lua_config->server_ip);
    }
    if (plugin->net_port == 0) {
        if (in_lua_config->server_port == 0)
            plugin->net_port = SERVPORT;
        else
            plugin->net_port = in_lua_config->server_port;
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
    ctx->buf = malloc(send_buf_size);
    ctx->buf_len = send_buf_size;

    struct command_connect_req_head req_head;
    memset (&req_head, 0 , sizeof(req_head));
    req_head.userid = 10;
    req_head.host = 0;
    req_head.version = 1;
    req_head.tlv_len = 0;
    storage_process_connect(ctx, &req_head);
    return 0;
}

int cb_lua_exit(void *data, struct flb_config *config) {
    flb_info("call cb_lua_exit");
    (void) config;
    struct flb_out_lua_config *ctx = data;
    lua_meta_destory(&ctx->m_list);
    //storage_process_connect_close(ctx);
    free(ctx);

    return 0;
}

int cb_lua_flush(void *data, size_t bytes, void *out_context,
                     struct flb_config *config) {
    flb_info("call out lua flush");
    (void) config;
    if (data == NULL) {
        flb_info("call out lua flush data is NULL");
        return 0;
    }
    struct flb_out_lua_config *ctx = (struct flb_out_lua_config*)out_context;
    int ret = -1;
    ret = parse_data_type(ctx, data, bytes);
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
