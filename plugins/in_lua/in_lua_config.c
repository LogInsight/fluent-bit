//
// Created by mayabin on 15/12/30.
//

#include <stdio.h>
#include <stdlib.h>
//#include <unistd.h>
#include <string.h>
//#include <sys/types.h>
//#include <unistd.h>
#include <dirent.h>
//#include <sys/epoll.h>


#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>


#include <msgpack.h>
#include <fluent-bit/flb_input.h>
//#include <fluent-bit/flb_config.h>
//#include <fluent-bit/flb_pack.h>
#include "in_lua.h"
#include "in_lua_config.h"


static struct flb_in_lua_callback gst_config_call[config_max] = {
        [config_file] = {"FILE", "file_", "File:", in_lua_file_conf},
        [config_exec] = {"EXEC", "exec_", "Exec:", in_lua_exec_conf},
        [config_stat] = {"STAT", "stat_", "Stat:", in_lua_stat_conf}
};

static struct flb_in_lua_global gst_global_config;

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
            else if (0 == strcasecmp(entry->key, "access_key")){
                gst_global_config.access_key = entry->val;
            }
            else if (0 == strcasecmp(entry->key, "host_key")){
                gst_global_config.host_key = entry->val;
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

