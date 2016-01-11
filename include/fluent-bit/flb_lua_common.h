#ifndef __FLUENT_BIT_LUA_COMMON_H__
#define __FLUENT_BIT_LUA_COMMON_H__


#include <fluent-bit/flb_config.h>
#include <fluent-bit/flb_input.h>
#include <fluent-bit/flb_utils.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

enum exec_type {
    exec_stdout = 0,
    exec_stderr,
    exec_both
};



struct flb_in_lua_file{
    char *journal_directory;
    char *log_directory;
    char *file_match;
    char *priority;
    int rescan_interval;
    struct mk_list *tags;
};

struct flb_in_lua_exec{
    uint8_t exec_type;
    int refresh_interval;
    char *shell;
    char *call;
    struct mk_list *tags;
};

struct flb_in_lua_stat{
    int refresh_interval;
    char *format;
};

struct flb_in_lua_global{
    uint32_t refresh_interval;
    uint32_t io_limit;
    uint32_t cpu_limit;
    uint32_t mem_size;
    uint32_t server_port;
    char *access_key;
    char *host_key;
    char *hostname;
    char *watch_mode;
    char *server_ip;
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
    uint32_t crc32;
    uint32_t stream_id;
};

struct flb_in_lua_exec_info {
    struct mk_list _head;
    struct flb_in_lua_exec exec_config;
    uint32_t stream_id[exec_both];
    uint32_t exec_fd[exec_both];
    uint64_t offset[exec_both];
};

struct flb_in_lua_stat_info {
    struct mk_list _head;
    struct flb_in_lua_stat stat_config;
};


struct flb_in_lua_global *in_lua_get_global();

struct mk_list *in_lua_get_file_head();

#endif