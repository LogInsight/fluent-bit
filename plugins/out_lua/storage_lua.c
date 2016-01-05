//
// Created by jc on 5/1/2016.
//

#include "storage_lua.h"
#include <msgpack.h>
#include <stdio.h>

#include <fluent-bit/flb_utils.h>
#include <fluent-bit/flb_io.h>


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
        plugin->net_host = strdup("127.0.0.1");
    }
    if (plugin->net_port == 0) {
        plugin->net_port = 12800;
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

    return 0;
}

int cb_lua_exit(void *data, struct flb_config *config)
{
    (void) config;
    struct flb_out_lua_config *ctx = data;
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
    //printf("the flush data = %s\n", (char *)data);

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
