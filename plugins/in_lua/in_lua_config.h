//
// Created by mayabin on 15/12/30.
//


#ifndef FLUENT_BIT_IN_LUA_CONFIG_H
#define FLUENT_BIT_IN_LUA_CONFIG_H


#include <fluent-bit/flb_config.h>
#include <fluent-bit/flb_input.h>
#include <fluent-bit/flb_utils.h>

struct flb_in_lua_file{
    char *journal_directory;
    char *log_directory;
    char *file_match;
    char *priority;
    int rescan_interval;
};

struct flb_in_lua_exec{
    int refresh_interval;
    char *watch;
    char *shell;
    char *call;
};

struct flb_in_lua_stat{
    int refresh_interval;
    char *format;
};

struct flb_in_lua_global{
    int refresh_interval;
    char *hostname;
    char *access_key;
    char *host_key;
    int io_limit;
    int cpu_limit;
    int mem_size;
};

struct flb_in_lua_file_info {
    struct mk_list _head;
    struct flb_in_lua_file file_config;
    char file_name[1024];
    int file_fd;
    bool changed;
    bool new_file;
    int wfd;
};

struct flb_in_lua_exec_info {
    struct mk_list _head;
    struct flb_in_lua_exec exec_config;
};

struct flb_in_lua_stat_info {
    struct mk_list _head;
    struct flb_in_lua_stat stat_config;
};

enum config_key{
    config_file = 0,
    config_exec,
    config_stat,
    config_max
};

typedef void (*in_lua_config_layer_two)(struct flb_in_lua_config *, struct mk_rconf *, char *);

struct flb_in_lua_callback{
    char *key;
    char *prefix;
    char *layer_prefix;
    in_lua_config_layer_two pfunc;
};

void in_lua_file_conf(struct flb_in_lua_config *, struct mk_rconf *conf, char *key);
void in_lua_exec_conf(struct flb_in_lua_config *, struct mk_rconf *conf, char *key);
void in_lua_stat_conf(struct flb_in_lua_config *, struct mk_rconf *conf, char *key);

void in_lua_get_file(struct flb_in_lua_config *ctx, struct flb_in_lua_file_info *file);

void in_lua_config(struct flb_in_lua_config* ctx, struct mk_rconf *conf);


#endif //FLUENT_BIT_IN_LUA_CONFIG_H
