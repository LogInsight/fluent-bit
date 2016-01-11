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
#include <fluent-bit/flb_lua_common.h>
#include "in_lua.h"
#include "in_lua_config.h"

#define IN_LUA_DEFAULT_IO_LIMIT    (20 << 20)
#define IN_LUA_DEFAULT_CPU_LIMIT   10
#define IN_LUA_DEFAULT_MEM_SIZE    (4 << 20)


static struct flb_in_lua_callback gst_config_call[config_max] = {
        [config_file] = {"FILE", "file_", "File:", in_lua_file_conf},
        [config_exec] = {"EXEC", "exec_", "Exec:", in_lua_exec_conf},
        [config_stat] = {"STAT", "stat_", "Stat:", in_lua_stat_conf}
};

static struct flb_in_lua_global gst_global_config;

struct flb_in_lua_global *in_lua_get_global() {
    return &gst_global_config;
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

    dir = opendir(file->file_config.log_directory);
    if (NULL == dir) {
        flb_warn("%s open error for %s.", file->file_config.log_directory, strerror(errno));
        return;
    }

    //luaL_loadfile(L, "test.lua");
    while (NULL != (ptr = readdir(dir))) {
        if (ptr->d_name[0] == '.') {
            continue;
        }
        if (ptr->d_type == 8){
            data_len = snprintf(lua_command, 4096, "return ('%s'):match('%s')", ptr->d_name, file->file_config.file_match);
            lua_command[data_len] = '\0';
            luaL_dostring(L, lua_command);
            if (first){
                second = lua_tostring(L, -1);
                lua_pop(L, 1);
                if (second){
                    if (file->file_config.priority) {
                        lua_pushstring(L, first);
                        lua_pushstring(L, second);
                        lua_getglobal(L, file->file_config.priority);
                        lua_pcall(L, 2, 1, 0);
                        first = lua_tostring(L, -1);
                        lua_pop(L, 1);
                    }
                    else {
                        if (0 > strcmp(second, first)) {
                            first = second;
                        }
                    }
                    flb_info("match file = %s", second);
                }
            }
            else {
                first = lua_tostring(L, -1);
                lua_pop(L, 1);
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
        flb_warn("can not find file to read in directory %s.", file->file_config.log_directory);
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
        mk_list_init(&file->_head);
        file->stream_id = 0;

        mk_list_foreach(head, &section->entries)
        {
            entry = mk_list_entry(head, struct mk_rconf_entry, _head);
            flb_info("section key = %s, val = %s", entry->key, entry->val);

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
                if (file->file_config.rescan_interval == 0) {
                    file->file_config.rescan_interval = gst_global_config.refresh_interval;
                }
            }
            else if(0 == strcasecmp(entry->key, "priority")) {
                file->file_config.priority = entry->val;
                lua_getglobal(ctx->lua_state ,entry->val);
                if (!(lua_isfunction(ctx->lua_state, -1))) {
                    fprintf(stderr, "priority (%s) in [%s] is not a function, please check the lua script.\n", entry->val, key);
                    exit(-1);
                }
                lua_pop(ctx->lua_state, 1);
            }
            else {
                flb_info("config [%s] not support %s.", key, entry->key);
            }
        }

        in_lua_get_file(ctx, file);
        mk_list_add(&file->_head, &ctx->file_config);
    }

    return;
}

void in_lua_exec_conf(struct flb_in_lua_config* ctx, struct mk_rconf *conf, char *key)
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
        //file->exec_config.call = NULL;
        file->exec_config.shell = NULL;
        file->exec_config.exec_type = exec_both;
        mk_list_init(&file->_head);


        mk_list_foreach(head, &section->entries)
        {
            entry = mk_list_entry(head, struct mk_rconf_entry, _head);
            flb_info("section key = %s, val = %s", entry->key, entry->val);
            if(0 == strcasecmp(entry->key, "watch")) {
                if (0 == strcasecmp(entry->val, "both")) {
                    //pass
                }
                else if (0 == strcasecmp(entry->val, "stdout")) {
                    file->exec_config.exec_type = exec_stdout;
                }
                else if (0 == strcasecmp(entry->val, "stderr")) {
                    file->exec_config.exec_type = exec_stderr;
                }
                else {
                    flb_error("match config error in [%s], the value should be one of [both | stdout | stderr]", key);
                }
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
                flb_info("config [%s] not support %s.", key, entry->key);
            }
        }

        mk_list_add(&file->_head, &ctx->exec_config);
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
        mk_list_init(&file->_head);

        mk_list_foreach(head, &section->entries)
        {
            entry = mk_list_entry(head, struct mk_rconf_entry, _head);
            flb_info("section key = %s, val = %s", entry->key, entry->val);

            if(0 == strcasecmp(entry->key, "format")) {
                file->stat_config.format = entry->val;
            }
            else if(0 == strcasecmp(entry->key, "refresh_interval")) {
                file->stat_config.refresh_interval = atoi(entry->val);
            }
            else {
                flb_info("config [%s] not support %s.", key, entry->key);
            }
        }
        mk_list_add(&file->_head, &ctx->stat_config);
    }
}

static void in_lua_ls_config(struct flb_in_lua_config* ctx, struct mk_rconf *conf)
{
    struct mk_rconf_section *section;
    struct mk_rconf_entry *entry;
    struct mk_list *head;
    struct mk_list *lua_path_head;
    struct mk_string_line *lua_path_entry;
    lua_State *L = ctx->lua_state;
    int status = 0;
    int resault = 0;
    int tmp;
    char *path = NULL;

    gst_global_config.hostname = NULL;
    gst_global_config.refresh_interval = 5;
    gst_global_config.cpu_limit = IN_LUA_DEFAULT_CPU_LIMIT;
    gst_global_config.io_limit = IN_LUA_DEFAULT_IO_LIMIT;
    gst_global_config.mem_size = IN_LUA_DEFAULT_MEM_SIZE;
    gst_global_config.watch_mode = "event";
    gst_global_config.server_ip = NULL;
    gst_global_config.server_port = 0;

    section = mk_rconf_section_get(conf, "LS");
    if (section)
    {
        lua_pushcfunction(L, luaopen_package);
        lua_pushstring(L, "");
        lua_call(L, 1, 0);

        mk_list_foreach(head, &section->entries)
        {
            entry = mk_list_entry(head, struct mk_rconf_entry, _head);
            flb_info("section key = %s, val = %s", entry->key, entry->val);

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
                    path = realpath(lua_path_entry->val, NULL);
                    if (path) {
                        flb_info("extend_lua_path lua_path = %s", lua_path_entry->val);
                        set_lua_path(L, lua_path_entry->val, (size_t) lua_path_entry->len);
                    }
                    else {
                        flb_warn("extend_lua_path failed. lua_path = %s", lua_path_entry->val);
                    }
                }
            }
            else if (0 == strcasecmp(entry->key, "lua_engine")){
                ctx->lua_engine = entry->val;
                flb_info("lua_execute lua_engine = %s", realpath(ctx->lua_engine, NULL));
            }
            else if (0 == strcasecmp(entry->key, "access_key")){
                gst_global_config.access_key = atoi(entry->val);
            }
            else if (0 == strcasecmp(entry->key, "host_key")){
                gst_global_config.host_key = atoi(entry->val);
            }
            else if(0 == strcasecmp(entry->key, "io_limit")){
                tmp = atoi(entry->val);
                if (tmp > 0){
                    gst_global_config.io_limit = tmp;
                }
                else {
                    flb_error("io_limit error in [LS], the value must bigger than 0.");
                }
            }
            else if(0 == strcasecmp(entry->key, "cpu_limit")){
                tmp = atoi(entry->val);
                if (0 < tmp && tmp <= 100)
                    gst_global_config.cpu_limit = tmp;
                else {
                    flb_error("cpu_limit error in [LS], the value must in (0, 100].");
                }
            }
            else if(0 == strcasecmp(entry->key, "mem_size")){
                tmp = atoi(entry->val);
                if (1 <= tmp && tmp <= 64)
                    gst_global_config.mem_size = tmp << 20;
                else {
                    flb_error("mem_size error in [LS], the value must in [1, 64].");
                }
            }
            else if (0 == strcasecmp(entry->key, "watch_mode")) {
                if (0 == strcasecmp(entry->val, "timer") || 0 == strcasecmp(entry->val, "event")){
                    gst_global_config.watch_mode = entry->val;
                }
                else {
                    flb_warn("watch_mode error in [LS], the value must be 'timer' or 'event'.");
                }
            }
            else if (0 == strcasecmp(entry->key, "server_ip")){
                gst_global_config.server_ip = entry->key;
            }
            else if (0 == strcasecmp(entry->key, "server_port")) {
                tmp = atoi(entry->key);
                if (tmp > 0 && tmp < 65536) {
                    gst_global_config.server_port = tmp;
                }
                else {
                    flb_warn("server_port error in [LS], the value must be in [1, 65535].");
                }
            }
            else {
                flb_warn("config [LS] not support %s.", entry->key);
            }
        }
    }
    if (ctx->lua_engine) {
        status = luaL_loadfile(L, ctx->lua_engine);
        if (status) {
            /* If something went wrong, error message is at the top of */
            /* the stack */
            fprintf(stderr, "Couldn't load file: %s", lua_tostring(L, -1));
            exit(1);
        }

        /* Ask Lua to run our little script */
        resault = lua_pcall(L, 0, LUA_MULTRET, 0);
        if (resault) {
            fprintf(stderr, "Failed to start script: %s", lua_tostring(L, -1));
            exit(1);
        }

        lua_getglobal(ctx->lua_state, "process");
        if (!(lua_isfunction(L, -1))) {
            flb_utils_error_c("lua_engine of [LS] does not has function process.");
        }
        lua_pop(ctx->lua_state, -1);
    }
    else {
        flb_utils_error_c("lua_engine not exit.");
    }

    if (strcasecmp(gst_global_config.watch_mode, "timer")) {
        ctx->timer_mode = MK_TRUE;
    }
    ctx->buf = (char *)malloc(gst_global_config.mem_size);
    ctx->buf_len = gst_global_config.mem_size;
    return;
}


void in_lua_config(struct flb_in_lua_config* ctx, struct mk_rconf *conf)
{
    if (NULL == conf)
    {
        flb_error("config is NULL.");
        return;
    }


    flb_info("call in_lua_config");
    int loop_num = 0;
    struct mk_rconf_section *section;
    struct mk_rconf_entry *entry;
    struct mk_list *head;

    in_lua_ls_config(ctx, conf);

    for (loop_num = 0; loop_num < config_max; loop_num ++) {
        section = mk_rconf_section_get(conf, gst_config_call[loop_num].key);
        if (section)
        {
            mk_list_foreach(head, &section->entries)
            {
                entry = mk_list_entry(head, struct mk_rconf_entry, _head);
                flb_info("FILE key = %s, val = %s", entry->key, entry->val);

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
                        flb_warn("config prefix of %s in [%s] is wrong. should be \"file_\"",
                               entry->key,
                               gst_config_call[loop_num].key);
                    }
                }
            }
        }
    }
}

