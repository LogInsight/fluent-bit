//
// Created by mayabin on 16/1/5.
//

#include <arpa/inet.h>
#include <fluent-bit/flb_lua_common.h>
#include "in_lua_exec.h"
#include "in_lua.h"
#include "in_lua_file.h"



static int g_stderr_pipe[2] = {};
static int g_stdout_pipe[2] = {};

static char *g_std_type[exec_both] = {
    [exec_stdout] = "stdout",
    [exec_stderr] = "stderr"
};

static void in_lua_exec_open_behave(struct flb_in_lua_config *ctx, struct flb_in_lua_exec_info *exec, uint8_t type) {
    int len = -1;
    COMMAND_OPEN_REQ_S stReq;
    struct stat stat;
    char buf[8192];
    uint32_t data_len = 0;
    uint32_t tlv_num = 0;
    struct mk_list *head;
    struct mk_string_line *entry;

    if (type >= exec_both) {
        return;
    }
    fstat(exec->exec_fd[type], &stat);

    len = snprintf(buf, 8192, "/dev/%s/%s",g_std_type[type], exec->exec_config.shell);
    data_len += len;

    stReq.create_timestamp = htonll(stat.st_ctime);
    stReq.modify_timestamp = htonll(stat.st_mtime);
    stReq.group = htonl(stat.st_gid);
    stReq.owner = htonl(stat.st_uid);
    stReq.mod = htonl(stat.st_mode);
    stReq.offset = htonll(exec->offset[type]);
    stReq.stream_id = htonl(exec->stream_id[type]);
    stReq.substream_id = 0;
    stReq.filename_len = htons(len);
    if (exec->exec_config.tags) {
        mk_list_foreach(head, exec->exec_config.tags) {
            entry = mk_list_entry(head, struct mk_string_line, _head);

            len = tlv_encode(FILE_TAG, (uint16_t) entry->len, entry->val, buf + data_len, 8192 - data_len);
            if (len > 0) {
                data_len += len;
                tlv_num++;
            }
        }
    }

    stReq.tlv_len = htonl(tlv_num);

    len = data_encode(COMMAND_STREAM_START,
                      &stReq,
                      sizeof(stReq),
                      (void *)buf,
                      data_len,
                      ctx->buf + ctx->read_len,
                      ctx->buf_len - ctx->read_len);

    if (len > 0) {
        ctx->read_len += len;
    }

    return;
}

static void in_lua_exec_stdout_open(struct flb_in_lua_config *ctx, struct flb_in_lua_exec_info *entry) {
    entry->exec_fd[exec_stdout] = g_stdout_pipe[0];
    g_stream_id ++;
    entry->stream_id[exec_stdout] = g_stream_id;
    in_lua_exec_open_behave(ctx, entry, exec_stdout);
    return;
}

static void in_lua_exec_stderr_open(struct flb_in_lua_config *ctx, struct flb_in_lua_exec_info *entry) {
    entry->exec_fd[exec_stderr] = g_stderr_pipe[0];
    g_stream_id ++;
    entry->stream_id[exec_stderr] = g_stream_id;
    in_lua_exec_open_behave(ctx, entry, exec_stderr);
    return;
}

void in_lua_exec_init(struct flb_in_lua_config *ctx) {
    int res = 0;
    struct flb_in_lua_exec_info *entry;
    struct mk_list *head;

    res = pipe(g_stderr_pipe);
    if (res < 0) {
        flb_utils_error_c("exec init failed.");
    }
    res = pipe(g_stdout_pipe);
    if (res < 0) {
        flb_utils_error_c("exec init failed.");
    }

    mk_list_foreach(head, &ctx->exec_config) {
        entry = mk_list_entry(head, struct flb_in_lua_exec_info, _head);
        switch (entry->exec_config.exec_type) {
            case exec_both:
                in_lua_exec_stdout_open(ctx, entry);
                in_lua_exec_stderr_open(ctx, entry);
                break;
            case exec_stderr:
                in_lua_exec_stderr_open(ctx, entry);
                break;
            case exec_stdout:
                in_lua_exec_stdout_open(ctx, entry);
                break;
            default:
                break;
        }
    }

    return;
}

void in_lua_exec_read(struct flb_in_lua_config *ctx, struct flb_in_lua_exec_info *exec) {
    pid_t pid = -1;

    if (NULL == exec->exec_config.shell) {
        return;
    }

    pid = fork();

    if (0 == pid) {
        switch (exec->exec_config.exec_type) {
            case exec_both:
                dup2(g_stdout_pipe[1], STDOUT_FILENO);
                dup2(g_stderr_pipe[1], STDERR_FILENO);
                break;
            case exec_stdout:
                dup2(g_stdout_pipe[1], STDOUT_FILENO);
                break;
            case exec_stderr:
                dup2(g_stderr_pipe[1], STDERR_FILENO);
                break;
            default:
                break;
        }

        close(g_stderr_pipe[0]);
        close(g_stderr_pipe[1]);
        close(g_stdout_pipe[0]);
        close(g_stdout_pipe[1]);

        system(exec->exec_config.shell);
    }
    else if (0 < pid) {
        switch (exec->exec_config.exec_type) {
            case exec_both:
                in_lua_read(ctx,
                            exec->exec_fd[exec_stdout],
                            &exec->offset[exec_stdout],
                            exec->stream_id[exec_stdout],
                            MK_FALSE);

                in_lua_read(ctx,
                            exec->exec_fd[exec_stderr],
                            &exec->offset[exec_stderr],
                            exec->stream_id[exec_stderr],
                            MK_FALSE);
                break;
            case exec_stdout:
                in_lua_read(ctx,
                            exec->exec_fd[exec_stdout],
                            &exec->offset[exec_stdout],
                            exec->stream_id[exec_stdout],
                            MK_FALSE);
                break;
            case exec_stderr:
                in_lua_read(ctx,
                            exec->exec_fd[exec_stderr],
                            &exec->offset[exec_stderr],
                            exec->stream_id[exec_stderr],
                            MK_FALSE);
                break;
            default:
                break;
        }
    }
    else {
        flb_error("exec fork error.");
    }
    return;
}
