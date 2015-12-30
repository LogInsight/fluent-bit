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


void in_lua_file_conf(struct flb_in_lua_config *, struct mk_rconf *conf, char *key);
void in_lua_exec_conf(struct flb_in_lua_config *, struct mk_rconf *conf, char *key);
void in_lua_stat_conf(struct flb_in_lua_config *, struct mk_rconf *conf, char *key);


static struct flb_in_lua_callback gst_config_call[config_max] = {
    [config_file] = {"FILE", "file_", "File:", in_lua_file_conf},
    [config_exec] = {"EXEC", "exec_", "Exec:", in_lua_exec_conf},
    [config_stat] = {"STAT", "stat_", "Stat:", in_lua_stat_conf}
};

static struct flb_in_lua_global gst_global_config;

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

void in_lua_get_file(struct flb_in_lua_config *ctx, struct flb_in_lua_file_info *file)
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

    lua_State *L = ctx->lua_state;

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
            data_len = snprintf(file->file_name, sizeof(file->file_name), "%s",first);
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

/* ----- lua helper ----- */
int set_lua_path( lua_State* L, const char* lua_path, size_t lua_path_length)
{
    lua_getglobal( L, "package" );
    lua_getfield( L, -1, "path" ); // get field "path" from table at top of stack (-1)
    const char* cur_path = lua_tostring( L, -1 ); // grab path string from top of stack
    size_t cur_path_length = strlen(cur_path);
    char* target_path = malloc(sizeof(char)*( cur_path_length + lua_path_length + 2 ));   // 1 -> ;  1->\0
    memcpy(target_path, cur_path, cur_path_length);
    target_path[cur_path_length] = ';';
    memcpy(&(target_path[cur_path_length+1]), lua_path, lua_path_length);
    target_path[cur_path_length + lua_path_length + 1] = 0;

    lua_pop( L, 1 ); // get rid of the string on the stack we just pushed on line 5
    lua_pushstring( L, target_path ); // push the new one
    lua_setfield( L, -2, "path" ); // set the field "path" in table at -2 with value at top of stack
    lua_pop( L, 1 ); // get rid of package table from top of stack
    return 0; // all done!
}

void in_lua_require(lua_State *L, const char *name, const char* global_name) {
    lua_getglobal(L, "require");
    lua_pushstring(L, name);
    lua_call(L, 1, 0);
    //lua_setglobal(L, global_name);
}

/* ------- do init [ file | exec | stat ] ------- */
void in_lua_file_conf(struct flb_in_lua_config *ctx, struct mk_rconf *conf, char *key)
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
        file->file_config.journal_directory = "/var/log/.lsight";
        file->file_config.log_directory = NULL;
        file->file_config.priority = NULL;
        file->file_config.rescan_interval = gst_global_config.refresh_interval;
        file->new_file = false;
        file->file_name[0] = '\0';
        file->file_fd = -1;

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

        in_lua_get_file(ctx, file);
        mk_list_add(&file->_head, ctx->file_config);
    }

    return;
}

#if 0
void in_lua_config(struct flb_in_lua_config* ctx, struct mk_rconf *conf) {
    /*
     * 从文件中加载 配置信息
     * */

    if (NULL == conf || NULL == ctx) {
        return;
    }
    struct flb_in_lua_exec_info *file;
    struct mk_rconf_section *section;
    struct mk_rconf_entry *entry;
    struct mk_list *head;

    /* 初始化系统的环境变量 */
    {
        int status, result;
        struct mk_string_line *lua_path_entry;
        lua_State *L = (lua_State *) ctx->lua_state;
        section = mk_rconf_section_get(conf, "LS");
        if (section) {
            /* Validate TD section keys */
            if (MK_TRUE == (size_t) mk_rconf_section_get_key(section, "lua_debug", MK_RCONF_BOOL)) {
                // luaopen_debug
                lua_pushcfunction(L, luaopen_debug);
                lua_pushstring(L, "");
                lua_call(L, 1, 0);
            }

            //if(MK_TRUE == (size_t)mk_rconf_section_get_key(section, "lua_package", MK_RCONF_BOOL)) {
            if (1) {
                // 此处必须为 on ,  因为没有 package 机制，就没有 require
                // luaopen_package
                lua_pushcfunction(L, luaopen_package);
                lua_pushstring(L, "");
                lua_call(L, 1, 0);
            }

            ctx->lua_paths = mk_rconf_section_get_key(section, "lua_path", MK_RCONF_LIST);
            // set package path
            mk_list_foreach(head, ctx->lua_paths) {
                lua_path_entry = mk_list_entry(head, struct mk_string_line, _head);
                flb_info("extend_lua_path lua_path = %s", lua_path_entry->val);
                set_lua_path(L, lua_path_entry->val, (size_t) lua_path_entry->len);
            }
            // end check path
        }
        /*
         * - 最初的设计为 通过 require 动态加载
         * - 实际实现为， 直接执行制定的 LUA 脚本， 读取脚本中、调用 定义的 全局函数
         * */
        ctx->lua_engine = mk_rconf_section_get_key(section, "lua_engine", MK_RCONF_STR);
        flb_info("lua_execute lua_engine = %s", ctx->lua_engine);
        status = luaL_loadfile(L, ctx->lua_engine);
        if (status) {
            /* If something went wrong, error message is at the top of */
            /* the stack */
            fprintf(stderr, "Couldn't load file: %s\n", lua_tostring(L, -1));
            exit(1);
        }
        /* TODO: Give the global object here */
        /* Ask Lua to run our little script */
        result = lua_pcall(L, 0, LUA_MULTRET, 0);
        if (result) {
            fprintf(stderr, "Failed to start script: %s\n", lua_tostring(L, -1));
            exit(1);
        }
        /* TODO: call LUA function from C */
        //in_lua_require(L, ctx->lua_engine, "_ls_engine");
    }
}
#endif
void in_lua_exec_conf(struct flb_in_lua_config* ctx, struct mk_rconf *conf, char *key)
{
    struct flb_in_lua_exec_info *file;
    struct mk_rconf_section *section;
    struct mk_rconf_entry *entry;
    struct mk_list *head;

    section = mk_rconf_section_get(conf, "EXEC");
    if (section)
    {

        file = (struct flb_in_lua_exec_info *)malloc(sizeof(struct flb_in_lua_exec_info));


        file->exec_config.refresh_interval = gst_global_config.refresh_interval;
        file->exec_config.call = NULL;
        file->exec_config.shell = NULL;
        file->exec_config.watch = NULL;


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

        mk_list_add(&file->_head, ctx->exec_config);
    }
}

void in_lua_stat_conf(struct flb_in_lua_config* ctx, struct mk_rconf *conf, char *key)
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
                flb_info("config [%s] not support %s.\r\n", key, entry->key);
            }
        }
        mk_list_add(&file->_head, ctx->stat_config);
    }
}

void in_lua_ls_config(struct flb_in_lua_config* ctx, struct mk_rconf *conf)
{
    struct mk_rconf_section *section;
    struct mk_rconf_entry *entry;
    struct mk_list *head;
    struct mk_list *lua_path_head;
    struct mk_string_line *lua_path_entry;
    lua_State *L = ctx->lua_state;
    int status = 0;
    int resault = 0;

    gst_global_config.hostname = NULL;
    gst_global_config.refresh_interval = 5;

    section = mk_rconf_section_get(conf, "LS");
    if (section)
    {
        flb_info("before luaopen_package");
        lua_pushcfunction(L, luaopen_package);
        flb_info("after luaopen_package");
        lua_pushstring(L, "");
        lua_call(L, 1, 0);

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
            else if (0 == strcasecmp(entry->key, "lua_debug") && 0 == strcasecmp(entry->val, "on")){
                // luaopen_debug
                lua_pushcfunction(L, luaopen_debug);
                lua_pushstring(L, "");
                lua_call(L, 1, 0);
            }
            else if (0 == strcasecmp(entry->key, "lua_path")) {
                ctx->lua_paths = mk_string_split_line(entry->val);
                mk_list_foreach(lua_path_head, ctx->lua_paths) {
                    lua_path_entry = mk_list_entry(head, struct mk_string_line, _head);
                    flb_info("extend_lua_path lua_path = %s", lua_path_entry->val);
                    set_lua_path(L, lua_path_entry->val, (size_t) lua_path_entry->len);
                }
            }
            else if (0 == strcasecmp(entry->key, "lua_engine")){
                flb_info("lua_execute lua_engine = %s", ctx->lua_engine);
                status = luaL_loadfile(L, ctx->lua_engine);
                if (status) {
                    /* If something went wrong, error message is at the top of */
                    /* the stack */
                    fprintf(stderr, "Couldn't load file: %s\n", lua_tostring(L, -1));
                    exit(1);
                }
                /* TODO: Give the global object here */
                /* Ask Lua to run our little script */
                resault = lua_pcall(L, 0, LUA_MULTRET, 0);
                if (resault) {
                    fprintf(stderr, "Failed to start script: %s\n", lua_tostring(L, -1));
                    exit(1);
                }

                /* TODO: call LUA function from C */
                //in_lua_require(L, ctx->lua_engine, "_ls_engine");
            }
            else {
                printf("config [LS] not support %s.\r\n", entry->key);
            }
        }
    }


}


void in_lua_config(struct flb_in_lua_config* ctx, struct mk_rconf *conf)
{
    if (NULL == conf)
    {
        return;
    }

    flb_info("do in_lua_config");

    int loop_num = 0;
    struct mk_rconf_section *section;
    struct mk_rconf_entry *entry;
    struct mk_list *head;

    in_lua_ls_config(ctx, conf);

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

                        gst_config_call[loop_num].pfunc(ctx, conf, entry->key);
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

int in_lua_get_pipe(struct flb_in_lua_config *ctx, struct mk_rconf *file)
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
        ctx->lua_paths = NULL;
        ctx->lua_state = NULL;
        {
            /* 初始化 LUA 环境 */
            lua_State *L = luaL_newstate();
            //luaL_openlibs(L);
            ctx->lua_state = L;
            flb_info("%s", L);
            // using as sanbox
            //luaopen_base(L);
            flb_info("b luaopen_base");
            lua_pushcfunction(L, luaopen_base);
            flb_info("a luaopen_base");
            lua_pushstring(L, "");
            lua_call(L, 1, 0);

            //luaopen_table(L);
            flb_info("b luaopen_table");
            lua_pushcfunction(L, luaopen_table);
            lua_pushstring(L, "");
            lua_call(L, 1, 0);

            //luaopen_string(L);
            lua_pushcfunction(L, luaopen_string);
            lua_pushstring(L, "");
            lua_call(L, 1, 0);

            //luaopen_math(L);
            lua_pushcfunction(L, luaopen_math);
            lua_pushstring(L, "");
            lua_call(L, 1, 0);

            //luaopen_io(L);
            //luaopen_os(L);
            // load _debug & _package in in_lua_config
        }

        in_lua_get_data(ctx->lua_state);
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

/* Cleanup lua input */
int in_lua_exit(void *in_context, struct flb_config *config)
{
    /* 必须关闭 LUA 虚拟机， 让 LUA 端 可以释放资源 */
    struct flb_in_lua_config *ctx = in_context;
    if(ctx->lua_state) {
        lua_State *L = ctx->lua_state;

        flb_debug("[in_lua] shutdown lua engine...");
        lua_close(L);
        ctx->lua_state = NULL;
    }
    /* free config->index_files */
    if (ctx->lua_paths) {
        mk_string_split_free(ctx->lua_paths);
    }

    /* clear msgpackbuf */
    msgpack_sbuffer_destroy(&ctx->mp_sbuf);
    // msgpack_packer_free // 因为 init 实际是 in-place 初始化， 此处不 free
    // msgpack_packer_init(&ctx->mp_pck, &ctx->mp_sbuf, msgpack_sbuffer_write);
    return 0;
}

/* Initialize plugin */
int in_lua_init(struct flb_config *config)
{
    int fd;
    int ret;
    struct flb_in_lua_config *ctx;

    flb_info("call in_lua_init");


    /* Allocate space for the configuration */
    ctx = malloc(sizeof(struct flb_in_lua_config));
    if (!ctx) {
        return -1;
    }


    /* read the configure */
    flb_info("in_lua_config before");

    /* initialize MessagePack buffers */
    msgpack_sbuffer_init(&ctx->mp_sbuf);
    msgpack_packer_init(&ctx->mp_pck, &ctx->mp_sbuf, msgpack_sbuffer_write);
    ctx->buffer_id = 0;

    /* Clone the standard input file descriptor */
    //fd = dup(STDOUT_FILENO);
    fd = in_lua_get_pipe(ctx, config->file);
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
        return NULL;

    sbuf = &ctx->mp_sbuf;
    *size = sbuf->size;
    buf = malloc(sbuf->size);
    if (!buf)
        return NULL;

    /* set a new buffer and re-initialize our MessagePack context */
    memcpy(buf, sbuf->data, sbuf->size);
    msgpack_sbuffer_destroy(&ctx->mp_sbuf);
    msgpack_sbuffer_init(&ctx->mp_sbuf);
    msgpack_packer_init(&ctx->mp_pck, &ctx->mp_sbuf, msgpack_sbuffer_write);

    ctx->buffer_id = 0;

    return buf;

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

