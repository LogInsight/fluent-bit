#ifndef __FLUENT_BIT_LUA_COMMON_H__
#define __FLUENT_BIT_LUA_COMMON_H__


#include <fluent-bit/flb_config.h>
#include <fluent-bit/flb_input.h>
#include <fluent-bit/flb_utils.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>



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
    int io_limit;
    int cpu_limit;
    int mem_size;
    char *hostname;
    char *access_key;
    char *host_key;
    char *watch_mode;
};

struct flb_in_lua_file_info {
    struct mk_list _head;
    struct flb_in_lua_file file_config;
    char file_name[1024];
    int file_fd;
    bool new_file;
    int wfd;
    uint64_t offset;
    struct stat file_stat;
};

struct flb_in_lua_exec_info {
    struct mk_list _head;
    struct flb_in_lua_exec exec_config;
};

struct flb_in_lua_stat_info {
    struct mk_list _head;
    struct flb_in_lua_stat stat_config;
};


struct flb_in_lua_global *in_lua_get_global();

struct mk_list *in_lua_get_file_head();

#endif