//
// Created by jc on 5/1/2016.
//

#include "storage_command.h"
#include "storage_type.h"
#include "storage_lua.h"
#include "storage_pack.h"

#include <fluent-bit/flb_utils.h>
#include <fluent-bit/flb_io.h>

bool storage_process_connect(struct flb_out_lua_config *ctx, void *head) {
    struct command_connect_req_head *req_head = (struct command_connect_req_head *) head;
    size_t buf_len = ctx->buf_len - 4;
    size_t req_len = 0;
    pack_command_connect(req_head, ctx->buf + 4, buf_len, &req_len);
    uint32_t pack_len = req_len;
    pack_len = ntohl(pack_len);
    memcpy(ctx->buf, (const void *)&pack_len, sizeof(uint32_t));
    ctx->buf_len = req_len + 4;

    size_t out_len = 0;
    flb_io_net_write(ctx->stream, ctx->buf, ctx->buf_len, &out_len);

    uint64_t recv_size = flb_io_net_read(ctx->stream, ctx->buf, send_buf_size);
    if (recv_size < 128 + sizeof(uint16_t)) {
        return false;
    }
    char *recv_body = ctx->buf + 128;
    uint16_t error_code;
    error_code = *(uint16_t*) recv_body;
    error_code = ntohs(error_code);
    if (error_code != RET_STATUS_OK) {
        flb_error("connect res error code = %u\n", error_code);
        return false;
    }
    return true;
}

bool storage_process_connect_close(struct flb_out_lua_config *ctx) {
    size_t req_len = send_buf_size;
    pack_command_close(ctx->buf + 4, send_buf_size - 4, &req_len);
    uint32_t pack_len = req_len;
    pack_len = ntohl(pack_len);
    memcpy(ctx->buf, (const void *)&pack_len, sizeof(uint32_t));
    ctx->buf_len = req_len + 4;

    size_t out_len = 0;
    flb_io_net_write(ctx->stream, ctx->buf, ctx->buf_len, &out_len);
    uint64_t recv_size = flb_io_net_read(ctx->stream, ctx->buf, send_buf_size);
    if (recv_size < sizeof(uint16_t)) {
        return false;
    }
    char *recv_body = ctx->buf;
    uint16_t error_code;
    error_code = *(uint16_t*) recv_body;
    error_code = ntohs(error_code);
    if (error_code != RET_STATUS_OK) {
        flb_error("connect close res error code = %u\n", error_code);
        return false;
    }
    return true;
}

static void
process_data_body(struct flb_out_lua_config *ctx, uint8_t type, const char *data, size_t len) {
    switch (type) {
        case DATA_PACK: {
            size_t send_size = 0;
            flb_io_net_write(ctx->stream, (void *)data, len, &send_size);
            if (len != send_size) {
                flb_error("io net wrie not complete");
            }
            uint64_t recv_size = flb_io_net_read(ctx->stream, ctx->buf, send_buf_size);
            if (recv_size < sizeof(uint16_t)) {
                break;
            }
            char *recv_body = ctx->buf;
            uint16_t error_code;
            error_code = *(uint16_t*) recv_body;
            error_code = ntohs(error_code);
            if (error_code != RET_STATUS_OK) {
                flb_error("connect res error code = %u\n", error_code);
                break;
            }
            break;
        }
        case COMMAND_STREAM_START: {
            size_t send_size = 0;
            flb_io_net_write(ctx->stream, (void *)data, len, &send_size);
            if (len != send_size) {
                flb_error("io net wrie not complete");
            }
            uint64_t recv_size = flb_io_net_read(ctx->stream, ctx->buf, send_buf_size);
            if (recv_size < sizeof(uint16_t)) {
                break;
            }
            char *recv_body = ctx->buf;
            uint16_t error_code;
            error_code = *(uint16_t*) recv_body;
            error_code = ntohs(error_code);
            if (error_code != RET_STATUS_OK) {
                flb_error("connect res error code = %u\n", error_code);
                break;
            }
            break;
        }
        case COMMAND_STREAM_END: {
            size_t send_size = 0;
            flb_io_net_write(ctx->stream, (void *)data, len, &send_size);
            if (len != send_size) {
                flb_error("io net wrie not complete");
            }
            uint64_t recv_size = flb_io_net_read(ctx->stream, ctx->buf, send_buf_size);
            if (recv_size < sizeof(uint16_t)) {
                break;
            }
            char *recv_body = ctx->buf;
            uint16_t error_code;
            error_code = *(uint16_t*) recv_body;
            error_code = ntohs(error_code);
            if (error_code != RET_STATUS_OK) {
                flb_error("connect res error code = %u\n", error_code);
                break;
            }
            break;
        }
        case COMMAND_STREAM: {
            size_t send_size = 0;
            flb_io_net_write(ctx->stream, (void *)data, len, &send_size);
            if (len != send_size) {
                flb_error("io net wrie not complete");
            }
            uint64_t recv_size = flb_io_net_read(ctx->stream, ctx->buf, send_buf_size);
            if (recv_size < sizeof(uint16_t)) {
                break;
            }
            char *recv_body = ctx->buf;
            uint16_t error_code;
            error_code = *(uint16_t*) recv_body;
            error_code = ntohs(error_code);
            if (error_code != RET_STATUS_OK) {
                flb_error("connect res error code = %u\n", error_code);
                break;
            }
            break;
        }
        case COMMAND_SUBSTREAM: {
            break;
        }
        case COMMAND_FILE: {
            size_t send_size = 0;
            flb_io_net_write(ctx->stream, (void *)data, len, &send_size);
            if (len != send_size) {
                flb_error("io net wrie not complete");
            }
            uint64_t recv_size = flb_io_net_read(ctx->stream, ctx->buf, send_buf_size);
            if (recv_size < sizeof(uint16_t)) {
                break;
            }
            char *recv_body = ctx->buf;
            uint16_t error_code;
            error_code = *(uint16_t*) recv_body;
            error_code = ntohs(error_code);
            if (error_code != RET_STATUS_OK) {
                flb_error("connect res error code = %u\n", error_code);
                break;
            }
            break;
        }
        case COMMAND_STREAM_INFO: {
            break;
        }
        case COMMAND_KEEP_ALIVE: {
            break;
        }
        default: {
            flb_error("not support this command type");
            break;
        }
    }
}

size_t parse_data_type(struct flb_out_lua_config *ctx, void *data, size_t len) {
    const char *data_ptr = (const char *)data;
    size_t data_pos = 0;
    while (data_pos < len) {
        uint32_t event_size = 0;
        if (data_pos + sizeof(uint32_t) < len) {
            break;
        }
        event_size = *(uint32_t *)data_ptr;
        data_pos += sizeof(uint32_t);
        if (data_pos + event_size < len) {
            break;
        }
        uint8_t type = 0;
        type = *(uint8_t*)data_ptr;
        data_ptr += sizeof(uint8_t);
        process_data_body(ctx, type, data_ptr, event_size - sizeof(uint8_t));
        data_pos += event_size;
    }
    return data_pos;
}