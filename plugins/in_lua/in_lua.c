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
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
//#include <sys/epoll.h>


#include <msgpack.h>
#include <fluent-bit/flb_input.h>
#include <fluent-bit/flb_config.h>
#include <fluent-bit/flb_pack.h>

#include "in_lua.h"

#define CONFIG_REREAD_INTERVAL        60

#define CONFIG_TABLE_NAME     "FLB_CONFIG_TABLE"

static int gai_pipe_fd_data[2] = {};
static int gai_pipe_fd_control[2] = {};


void in_lua_file_conf(lua_State *, struct mk_rconf *conf, char *key);
void in_lua_exec_conf(lua_State *, struct mk_rconf *conf, char *key);
void in_lua_stat_conf(lua_State *, struct mk_rconf *conf, char *key);


static struct flb_in_lua_callback gst_config_call[config_max] = {
    [config_file] = {"FILE", "file_", "File:", in_lua_file_conf},
    [config_exec] = {"EXEC", "exec_", "Exec:", in_lua_exec_conf},
    [config_stat] = {"STAT", "stat_", "Stat:", in_lua_stat_conf}
};

static struct flb_in_lua_global gst_global_config;
static struct flb_in_lua_file_info *g_file_info = NULL;
static struct flb_in_lua_exec_info *g_exec_info = NULL;
static struct flb_in_lua_stat_info *g_stat_info = NULL;

static uint32_t time_click_num = 0;

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

//int in_lua_config_record(lua_State *L, )

void in_lua_get_file(lua_State *L, struct flb_in_lua_file_info *file)
{
    if (file->file_config.log_directory == NULL){
        return;
    }

    if (file->file_config.file_match == NULL){
        return;
    }

    DIR *dir = NULL;
    struct dirent *ptr;
    char lua_command[4096] = {};
    int data_len = 0;

    const char *first = NULL;
    const char *second = NULL;

    dir = opendir(file->file_config.journal_directory);
    if (NULL == dir){
        printf("%s open error for %s.\r\n", file->file_config.journal_directory, strerror(errno));
        return;
    }
    closedir(dir);

    dir = opendir(file->file_config.log_directory);
    if (NULL == dir) {
        printf("%s open error for %s.\r\n", file->file_config.log_directory, strerror(errno));
        return;
    }

    luaL_loadfile(L, "test.lua");

    while (NULL != (ptr = readdir(dir))) {
        if (ptr->d_name[0] == '.') {
            continue;
        }
        if (ptr->d_type == 8){
            data_len = snprintf(lua_command, 4096, "('%s'):match('%s')", ptr->d_name, file->file_config.file_match);
            lua_command[data_len] = '\0';
            luaL_dostring(L, lua_command);
            if (first){
                second = lua_tostring(L, 1);
                if (second && file->file_config.priority) {
                    lua_pushstring(L, first);
                    lua_pushstring(L, second);
                    lua_getglobal(L, file->file_config.priority);
                    lua_pcall(L, 2, 1, 0);
                    first = lua_tostring(L, 1);
                }
            }
            else {
                first = lua_tostring(L, 1);
            }
        }
    }

    if (first) {
        if (0 != strcmp(file->file_name, first)) {
            data_len = snprintf(file->file_name, sizeof(file->file_name), first);
            file->file_name[data_len] = '\0';
            file->new_file = true;
        }
    }
    else {
        printf("can not find file to read in directory %s.\r\n", file->file_config.log_directory);
    }

    closedir(dir);

    return;
}

void in_lua_file_conf(lua_State *L, struct mk_rconf *conf, char *key)
{

    struct flb_in_lua_file_info *file;
    struct mk_rconf_section *section;
    struct mk_rconf_entry *entry;
    struct mk_list *head;

    section = mk_rconf_section_get(conf, key);
    if (section)
    {

        file = (struct flb_in_lua_file_info *)malloc(sizeof(struct flb_in_lua_file_info));
        file->file_config.file_match = NULL;
        file->file_config.journal_directory = "/var/log/lsight";
        file->file_config.log_directory = NULL;
        file->file_config.priority = NULL;
        file->file_config.rescan_interval = gst_global_config.refresh_interval;
        file->bool = false;
        file->file_name[0] = '\0';
        file->file_fd = -1;
        file->next = NULL;

        mk_list_foreach(head, &section->entries)
        {
            entry = mk_list_entry(head, struct mk_rconf_entry, _head);
            printf("section key = %s, val = %s\n", entry->key, entry->val);

            if(0 == strcasecmp(entry->key, "journal_directory")) {
                file->file_config.journal_directory = realpath(entry->val, NULL);
            }
            else if(0 == strcasecmp(entry->key, "log_directory")) {
                file->file_config.log_directory = realpath(entry->val, NULL);
            }
            else if(0 == strcasecmp(entry->key, "file_match")) {
                file->file_config.file_match = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "rescan_interval")) {
                file->file_config.rescan_interval = atoi(entry->val);
            }
            else if(0 == strcasecmp(entry->key, "priority")) {
                file->file_config.priority = entry->val;
            }
            else {
                printf("config [%s] not support %s.\r\n", key, entry->key);
            }
        }

        in_lua_get_file(L, file);
        if (NULL != g_file_info){
            file->next = g_file_info->next;
        }
        g_file_info = file;
    }

    return;
}

void in_lua_exec_conf(lua_State *L, struct mk_rconf *conf, char *key)
{

    struct flb_in_lua_exec_info *file;
    struct mk_rconf_section *section;
    struct mk_rconf_entry *entry;
    struct mk_list *head;


    section = mk_rconf_section_get(conf, key);
    if (section)
    {

        file = (struct flb_in_lua_exec_info *)malloc(sizeof(struct flb_in_lua_exec_info));

        file->exec_config.refresh_interval = gst_global_config.refresh_interval;
        file->exec_config.call = NULL;
        file->exec_config.shell = NULL;
        file->exec_config.watch = NULL;
        file->next = NULL;

        mk_list_foreach(head, &section->entries)
        {
            entry = mk_list_entry(head, struct mk_rconf_entry, _head);
            printf("section key = %s, val = %s\n", entry->key, entry->val);
            if(0 == strcasecmp(entry->key, "watch")) {
                file->exec_config.watch = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "shell")) {
                file->exec_config.shell = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "call")) {
                file->exec_config.call = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "refresh_interval")) {
                file->exec_config.refresh_interval = atoi(entry->val);
            }
            else {
                printf("config [%s] not support %s.\r\n", key, entry->key);
            }
        }

        if (NULL != g_exec_info){
            file->next = g_exec_info->next;
        }
        g_exec_info = file;
    }
}

void in_lua_stat_conf(lua_State *L, struct mk_rconf *conf, char *key)
{

    struct flb_in_lua_stat_info *file;
    struct mk_rconf_section *section;
    struct mk_rconf_entry *entry;
    struct mk_list *head;

    section = mk_rconf_section_get(conf, key);
    if (section)
    {
        file = (struct flb_in_lua_stat_info *)malloc(sizeof(struct flb_in_lua_stat_info));
        file->stat_config.refresh_interval = gst_global_config.refresh_interval;
        file->stat_config.format = NULL;
        file->next = NULL;

        mk_list_foreach(head, &section->entries)
        {
            entry = mk_list_entry(head, struct mk_rconf_entry, _head);
            printf("section key = %s, val = %s\n", entry->key, entry->val);

            if(0 == strcasecmp(entry->key, "format")) {
                file->stat_config.format = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "refresh_interval")) {
                file->stat_config.refresh_interval = atoi(entry->val);
            }
            else {
                printf("config [%s] not support %s.\r\n", key, entry->key);
            }
        }

        if (NULL != g_stat_info) {
            file->next = g_stat_info;
        }
        g_stat_info = file;
    }
}

void in_lua_global_config(lua_State *L, struct mk_rconf *conf)
{
    struct mk_rconf_section *section;
    struct mk_rconf_entry *entry;
    struct mk_list *head;

    gst_global_config.hostname = NULL;
    gst_global_config.refresh_interval = 5;

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

void in_lua_config(lua_State *L, struct mk_rconf *conf)
{
    if (NULL == conf)
    {
        return;
    }

    int loop_num = 0;
    struct mk_rconf_section *section;
    struct mk_rconf_entry *entry;
    struct mk_list *head;

    in_lua_global_config(L, conf);

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

                        gst_config_call[loop_num].pfunc(L, conf, entry->key);
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

void in_lua_get_data(lua_State *L)
{
    lua_register(L, "write", pipe_write_data);
    lua_register(L, "read", pipe_read_control);
    sleep(1);
    luaL_loadfile(L, "/home/ma/work/fluent-bit/test/test.lua");

    //创建epoll
    //int epoll_fd = epoll_create()

}

int in_lua_get_pipe(struct mk_rconf *file)
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


        lua_State *L = luaL_newstate();
        luaL_openlibs(L);
        luaL_loadfile(L, "../lua/init.lua");
        in_lua_config(L, file);
        in_lua_get_data(L);
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
    fd = in_lua_get_pipe(config->file);
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

