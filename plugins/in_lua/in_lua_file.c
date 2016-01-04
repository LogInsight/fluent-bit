//
// Created by mayabin on 15/12/30.
//

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <time.h>

#include <fluent-bit/flb_utils.h>
#include <sys/inotify.h>

#include "in_lua.h"
#include "in_lua_config.h"
#include "in_lua_file.h"

static int g_epoll_fd = -1;
static int g_inotify_fd = -1;

typedef enum tag_file_event_e {
    FILE_EVENT_CREATE = 0,
    FILE_EVENT_DELETE,
    FILE_EVENT_MODIFY,
    FILE_EVENT_MOVE_FROM,
    FILE_EVENT_MOVE_TO,
    FILE_EVENT_MAX
}FILE_EVENT_E;

typedef void (*file_event_callback) (struct flb_in_lua_config, struct flb_in_lua_file_info);

void in_lua_add_watch(struct flb_in_lua_config *ctx, struct flb_in_lua_file_info *file)
{
    struct mk_list *head;
    struct flb_in_lua_file_info *entry;
    int wfd = -1;
    mk_list_foreach(head, &ctx->file_config){
        entry = mk_list_entry(head, struct flb_in_lua_file_info, _head);
        if (entry == file)
        {
            wfd = inotify_add_watch(g_i_inotify_fd,
                                    entry->file_config.log_directory,
                                    IN_MOVE | IN_CREATE | IN_MODIFY | IN_DELETE);

            if (wfd < 0) {
                flb_warn("add watch failed in path : %s.", path);
                break;
            }
            entry->wfd = wfd;
            break;
        }
        else if (0 == strcmp(entry->file_config.log_directory, file->file_config.log_directory))
        {
            entry->wfd = wfd;
            break;
        }

    }
    return;
}

//move in
static void in_lua_file_move_to(struct flb_in_lua_config *ctx, struct inotify_event *event) {
    struct flb_in_lua_file_info *entry;
    struct flb_in_lua_file_info *tmp == NULL;
    struct mk_list *head;
    char buf[4096];
    uint32_t *len = buf;
    int data_len = 0;

    mk_list_foreach(head, ctx->file_config) {
        entry = mk_list_entry(head, struct flb_in_lua_file_info, _head);
        if (entry->wfd == wfd)
        {
            entry->new_file = true;
            tmp = entry;
        }
    }

    if (tmp) {
        data_len += snprintf(&buf[5],
                            4090,
                            "%s moved in %s, cookie = %d\n",
                            event->name,
                            tmp->file_config.log_directory,
                            event->cookie);
        buf[data_len] = '\0';
        *len = htonl(data_len - 4);
        *(char *)(len + 1) = COMMAND_FILE;
        in_lua_data_write(buf, data_len);
    }
    return;
}

//move out
static void in_lua_file_move_from(struct flb_in_lua_config *ctx, struct inotify_event *event) {
    struct flb_in_lua_file_info *entry;
    struct flb_in_lua_file_info *tmp = NULL;
    struct mk_list *head;
    char buf[4096];
    uint32_t *len = buf;
    uint32_t data_len = 5;

    mk_list_foreach(head, ctx->file_config) {
        entry = mk_list_entry(head, struct flb_in_lua_file_info, _head);
        if (entry->wfd == event->wfd)
        {
            tmp = entry;
            if (entry->file_fd != -1  && 0 == strcmp(entry->file_name, event->name))
            {
                close(entry->file_fd);
                entry->file_fd = -1;
                entry->new_file = true;
                break;
            }
        }
    }
    if (tmp) {
        data_len += snprintf(&buf[5],
                             4090,
                             "%s moved from %s, cookie = %d\n",
                             event->name,
                             tmp->file_config.log_directory,
                             event->cookie);
        buf[data_len] = '\0';
        *len = htonl(data_len - 4);
        *(char *)(len + 1) = COMMAND_FILE;
        in_lua_data_write(buf, data_len);
    }
    return;
}

static void in_lua_file_create(struct flb_in_lua_config *ctx, struct inotify_event *event) {
    struct flb_in_lua_file_info *entry;
    struct flb_in_lua_file_info *tmp = NULL;
    struct mk_list *head;
    char buf[4096];
    uint32_t data_len = 5;
    uint32_t *len = buf;

    mk_list_foreach(head, ctx->file_config) {
        entry = mk_list_entry(head, struct flb_in_lua_file_info, _head);
        if (entry->wfd == event->wfd)
        {
            tmp = entry;
            entry->new_file = ture;
            break;
        }
    }

    if (tmp) {
        data_len += snprintf(&buf[5],
                             4090,
                             "%s create in %s.\n",
                             event->name,
                             tmp->file_config.log_directory);
        buf[data_len] = '\0';
        *len = htonl(data_len - 4);
        *(char *)(len + 1) = COMMAND_FILE;
        in_lua_data_write(buf, data_len);
    }

    return;
}

static void in_lua_file_delete(struct flb_in_lua_config *ctx, struct inotify_event *event) {
    struct flb_in_lua_file_info *entry;
    struct flb_in_lua_file_info *tmp = NULL;
    struct mk_list *head;
    char buf[4096];
    uint32_t data_len = 5;
    uint32_t *len = buf;

    mk_list_foreach(head, ctx->file_config) {
        entry = mk_list_entry(head, struct flb_in_lua_file_info, _head);
        if (entry->wfd == event->wfd)
        {
            tmp = entry;
            if (0 == strcmp(entry->file_name, event->name) && entry->file_fd != -1){

                close(entry->file_fd);
                entry->file_fd = -1;
                entry->new_file = ture;
                break;
            }

        }
    }

    if (tmp) {
        data_len += snprintf(&buf[5],
                             4090,
                             "%s delete from %s.\n",
                             event->name,
                             tmp->file_config.log_directory);
        buf[data_len] = '\0';
        *len = htonl(data_len - 4);
        *(char *)(len + 1) = COMMAND_FILE;
        in_lua_data_write(buf, data_len);
    }

    return;
}

static void in_lua_file_modify(struct flb_in_lua_config *ctx, struct inotify_event *event) {
    struct flb_in_lua_file_info *entry;
    struct mk_list *head;

    mk_list_foreach(head, ctx->file_config) {
        entry = mk_list_entry(head, struct flb_in_lua_file_info, _head);
        if (entry->wfd == event->wfd && 0 == strcmp(entry->file_name, event->name) && entry->file_fd != -1)
        {
            entry->changed = true;
            break;
        }
    }
    return;
}

static file_event_callback g_file_event_callback[FILE_EVENT_MAX] = {
    [FILE_EVENT_CREATE] = in_lua_file_create,
    [FILE_EVENT_DELETE] = in_lua_file_delete,
    [FILE_EVENT_MODIFY] = in_lua_file_modify,
    [FILE_EVENT_MOVE_FROM] = in_lua_file_move_form,
    [FILE_EVENT_MOVE_TO] = in_lua_file_move_to,
};

static int in_lua_file_event(struct flb_in_lua_config *ctx, int i_watch_fd)
{
    int i_read_len = -1;
    int i_num = 0;
    char sz_buf[4096];
    //unsigned int uiCurrentTime = LIMIT_GetCurrentTime();
    sz_buf[0] = '\0';

    struct inotify_event *pst_inotify_event = NULL;
    int i_offset = 0;
    for (;;)
    {
        i_read_len = read(i_watch_fd, sz_buf, 4095);
        if (i_read_len <= 0)
        {
            break;
        }
        sz_buf[i_read_len] = '\0';
        while(i_offset < i_read_len)
        {
            pst_inotify_event = (struct inotify_event *)&sz_buf[i_offset];
            if (pst_inotify_event->len > 0) {
                 switch (pst_inotify_event->mask) {
                     case IN_MODIFY:{
                         in_lua_file_modify(ctx, pst_inotify_event);
                         break;
                     }
                     case IN_CREATE:{
                         in_lua_file_create(ctx, pst_inotify_event);
                         break;
                     }
                     case IN_DELETE:{
                         in_lua_file_delete(ctx, pst_inotify_event);
                         break;
                     }
                     case IN_MOVE_FORM:{
                         in_lua_file_move_from(ctx, pst_inotify_event);
                         break;
                     }
                     case IN_MOVE_TO: {
                         in_lua_file_move_to(ctx, pst_inotify_event);
                         break;
                     }
                     default:{
                         break;
                     }
                 }
            }
            i_offset += sizeof(struct inotify_event) + pst_inotify_event->len;
        }
    }
    //LIMIT_CheckCPULimit(uiCurrentTime, LIMIT_GetCurrentTime());
    return 1;
}

int in_lua_read_event(struct flb_in_lua_config *ctx) {
    int event_num = -1;
    struct epoll_event st_epoll_event;
    //memset(&st_epoll_event, 0, sizeof(st_epoll_event));

    event_num = epoll_wait(g_i_epoll_fd, &st_epoll_event, 1, 0);

    if (event_num > 0)
    {
        return file_data_get(ctx, st_epoll_event.data.fd);
    }
}

int file_open_behave(int fd, int offset, int stream_id, int sub_stream_id, const char *file_name)
{
    //printf("file_open_behave.\r\n");
    int iRecv = -1;
    char szBuf[4096];
    COMMAND_OPEN_REQ_S stReq;
    struct stat stFileStat;
    fstat(fd, &stFileStat);

    stReq.create_timestamp = htonl(stFileStat.st_ctime);
    stReq.modify_timestamp = htonl(stFileStat.st_mtime);
    stReq.group = htonl(stFileStat.st_gid);
    stReq.owner = htonl(stFileStat.st_uid);
    stReq.mod = htonl(stFileStat.st_mode);
    stReq.offset = htonl(offset);
    stReq.stream_id = htonl(stream_id);
    stReq.substream_id = htonl(sub_stream_id);
    stReq.tlv_len = 0;
    stReq.filename_len = htons(strlen(file_name));

    iRecv = data_encode(COMMAND_STREAM_START, &stReq, sizeof(stReq), (void *)file_name, ntohs(stReq.filename_len), szBuf, 4096);

    in_lua_data_write(szBuf, iRecv);
    return 0;
}


int in_lua_file_open(char *path, uint32_t offset)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        printf("open file %s failed.\r\n", path);
        return -1;
    }

    if (offset > 0)
    {
        lseek(fd, offset, SEEK_SET);
    }

    file_open_behave(fd, offset, fd, 0, path);

    return fd;
}

void in_lua_file_init(struct flb_in_lua_config *ctx)
{
    char file_name[4096];
    int len = 0;

    struct mk_list *head
    struct flb_in_lua_file_info *file;
    struct epoll_event  st_epoll_event;
    int wfd = -1;
    int file_fd = -1;

    g_epoll_fd = epoll_create(1024);

    if (g_epoll_fd == -1){
        flb_utils_error_c("file init failed for epoll create.");
    }

    g_inotify_fd = inotify_init1(IN_NONBLOCK);
    if (g_inotify_fd == -1) {
        flb_utils_error_c("file init failed for inotify init.")
    }

    st_epoll_event.events = EPOLLIN|EPOLLHUP;
    st_epoll_event.data.fd = g_inotify_fd;
    epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, g_inotify_fd, &st_epoll_event);

    mk_list_init(&g_wfd_record);


    file_name[0] = '\0';

    mk_list_foreach(head, &ctx->file_config) {
        file = mk_list_entry(head, struct flb_in_lua_config, _head);
        in_lua_add_watch(ctx, file);

        if(file->new_file) {
            //与路径整合
            len = snprintf(file_name, 4096, "%s/%s", file->file_config.log_directory, file->file_name);
            file_name[len] = '\0';
            //获取对应的meta信息
            //打开文件
            file_fd = in_lua_file_open(file_name, 0);
            file->file_fd = file_fd;
            file->changed = true;
        }
    }
}

void in_lua_timer_create(){
    struct itimerspec new_value;
    int max_exp, fd;
    struct timespec now;
    uint64_t exp, tot_exp;
    ssize_t s;


    if (clock_gettime(CLOCK_REALTIME, &now) == -1)
        flb_utils_error_c("clock_gettime failed.");

    /* Create a CLOCK_REALTIME absolute timer with initial
       expiration and interval as specified in command line */

    new_value.it_value.tv_sec = now.tv_sec + 2;
    new_value.it_value.tv_nsec = now.tv_nsec;
    new_value.it_interval.tv_sec = 1;
    new_value.it_interval.tv_nsec = 0;

    fd = timerfd_create(CLOCK_REALTIME, 0);
    if (fd == -1)
        flb_utils_error_c("timerfd_create failed.");

    if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &new_value, NULL) == -1)
        flb_utils_error_c("timerfd_settime failed.");

    return fd;
}

int file_stream_begin_behave(unsigned char ucType, int uiStreamId)
{
    //printf("file_stream_begin_behave. type = %d\r\n", ucType);
    int iRecv = -1;
    //unsigned int uiCurrentTime = LIMIT_GetCurrentTime();
    char szBuf[4096];
    COMMAND_STREAM_REQ_S stReq;

    stReq.stream_id = htonl(uiStreamId);
    stReq.tlv_len = 0;

    iRecv = data_encode(ucType, &stReq, sizeof(stReq), NULL, 0, szBuf, 4096);

    //LIMIT_CheckCPULimit(uiCurrentTime, LIMIT_GetCurrentTime());

    in_lua_data_write(szBuf, iRecv);

    return 0;
}

int file_stream_behave(unsigned int uiStreamId)
{
    return file_stream_begin_behave(COMMAND_STREAM, uiStreamId);
}


void in_lua_file_read(struct flb_in_lua_config *ctx, u_int64_t times) {
    struct mk_list *head
    struct flb_in_lua_file_info *file;
    static current_fd = -1;
    lua_State *L = ctx->lua_state;

    int read_len;

    mk_list_foreach(head, &ctx->file_config) {
        file = mk_list_entry(head, struct flb_in_lua_config, _head);
        in_lua_add_watch(ctx, file);

        if(file->changed && file->file_fd != -1 && 0 == times % file->file_config.rescan_interval) {
            read_len = read(file->file_fd, ctx->buf, ctx->buf_len);
            if (read_len > 0){
                if (current_fd != file->file_fd){
                    file_stream_behave(file->file_fd);
                }

            }
            else {
                file->changed = false;
            }
        }
    }
}