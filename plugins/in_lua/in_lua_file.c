//
// Created by mayabin on 15/12/30.
//

//#include <sys/timerfd.h>
//#include <time.h>

#include <fluent-bit/flb_utils.h>
#include <sys/inotify.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <mk_core/mk_event.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <unistd.h>

#include "in_lua.h"
#include "in_lua_config.h"
#include "in_lua_file.h"


static int g_i_inotify_fd= -1;

static uint32_t g_buf_len = 0;
static char *g_buf = NULL;

#define MAX_DATA_LEN   (64 << 10)

//typedef void (*file_event_callback) (struct flb_in_lua_config, struct flb_in_lua_file_info);

int file_stream_behave(struct flb_in_lua_file_info *file, struct flb_in_lua_config *ctx);

int tlv_encode(TLV_HEAD_S *pstHead, void *pBuf, unsigned int uiBufLen)
{
    unsigned int uiLen = sizeof(pstHead->stTl) + pstHead->stTl.len;

    if(uiLen > uiBufLen)
    {
        return -1;
    }
    memcpy(pBuf, pstHead, sizeof(pstHead->stTl));
    memcpy(pBuf + sizeof(pstHead->stTl), pstHead->pcValue, pstHead->stTl.len);
    return uiLen;
}

int data_encode(unsigned char ucType,
                void *pHead,
                unsigned  int uiHeadLen,
                void *pBuf,
                unsigned int uiBufLen,
                void *pOutBuf,
                unsigned int uiOutBufLen)
{
    char *pcCurrent = pOutBuf;
    unsigned int uiLen = 1 + uiHeadLen + uiBufLen;
//    printf("data len: %d \r\n", uiLen);

    if (uiLen > uiOutBufLen)
    {
        return -1;
    }

    *((unsigned int *)pcCurrent) = htonl(uiLen);
    pcCurrent += sizeof(int);
    *pcCurrent = ucType;
    pcCurrent ++;

    memcpy(pcCurrent, pHead, uiHeadLen);

    if (uiBufLen > 0)
    {
        memcpy(pcCurrent + uiHeadLen, pBuf, uiBufLen);
    }
    return uiLen + 4;
}


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
                flb_warn("add watch failed in path : %s.", entry->file_config.log_directory);
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

int file_close_behave(struct flb_in_lua_config *ctx, struct flb_in_lua_file_info *file)
{
    //printf("file_close_behave.\r\n");
    int iRecv = -1;
    COMMAND_STREAM_END_REQ_S stReq;

    stReq.stream_id = htonl(file->file_fd);
    iRecv = data_encode(COMMAND_STREAM_END,
                        &stReq,
                        sizeof(stReq),
                        NULL,
                        0,
                        ctx->buf + ctx->read_len,
                        ctx->buf_len - ctx->read_len);

    if (iRecv > 0) {
        ctx->read_len += iRecv;
    }

    return 0;
}


void in_lua_file_close(struct flb_in_lua_config *ctx, struct flb_in_lua_file_info *file) {
    if (file->file_fd == -1) {
        return;
    }

    file_close_behave(ctx, file);
    close(file->file_fd);
    file->file_fd = -1;
    file->new_file = MK_TRUE;
    file->offset = 0;

}

//move in
static void in_lua_file_move_to(struct flb_in_lua_config *ctx, struct inotify_event *event) {
    struct flb_in_lua_file_info *entry;
    struct flb_in_lua_file_info *tmp = NULL;
    struct mk_list *head;
    char buf[4096];
    int data_len = 0;
    FILE_HEAD_REQ_S file_head;

    mk_list_foreach(head, &ctx->file_config) {
        entry = mk_list_entry(head, struct flb_in_lua_file_info, _head);
        if (entry->wfd == event->wd)
        {
            entry->new_file = MK_TRUE;
            tmp = entry;
        }
    }

    if (tmp) {
        data_len += snprintf(buf,
                            4096,
                            "%s moved in %s, cookie = %d\n",
                            event->name,
                            tmp->file_config.log_directory,
                            event->cookie);
        buf[data_len] = '\0';
        file_head.len = htonl(data_len);

        data_len = data_encode(COMMAND_FILE,
                               &file_head,
                               sizeof(file_head),
                               buf,
                               data_len,
                               ctx->buf + ctx->read_len,
                               ctx->buf_len - ctx->read_len);
        if (data_len > 0) {
            ctx->read_len += data_len;
        }
    }
    return;
}


//move out
static void in_lua_file_move_from(struct flb_in_lua_config *ctx, struct inotify_event *event) {
    struct flb_in_lua_file_info *entry;
    struct flb_in_lua_file_info *tmp = NULL;
    struct mk_list *head;
    char buf[4096];
    uint32_t data_len = 0;
    FILE_HEAD_REQ_S file_head;

    mk_list_foreach(head, &ctx->file_config) {
        entry = mk_list_entry(head, struct flb_in_lua_file_info, _head);
        if (entry->wfd == event->wd)
        {
            tmp = entry;
            if (entry->file_fd != -1  && 0 == strcmp(entry->file_name, event->name))
            {
                in_lua_file_close(ctx, entry);
                break;
            }
        }
    }
    if (tmp) {
        data_len += snprintf(buf,
                             4096,
                             "%s moved from %s, cookie = %d\n",
                             event->name,
                             tmp->file_config.log_directory,
                             event->cookie);
        buf[data_len] = '\0';
        file_head.len = htonl(data_len);

        data_len = data_encode(COMMAND_FILE,
                               &file_head,
                               sizeof(file_head),
                               buf,
                               data_len,
                               ctx->buf + ctx->read_len,
                               ctx->buf_len - ctx->read_len);
        if (data_len > 0) {
            ctx->read_len += data_len;
        }
    }
    return;
}

static void in_lua_file_create(struct flb_in_lua_config *ctx, struct inotify_event *event) {
    struct flb_in_lua_file_info *entry;
    struct flb_in_lua_file_info *tmp = NULL;
    struct mk_list *head;
    char buf[4096];
    uint32_t data_len = 0;
    FILE_HEAD_REQ_S file_head;

    mk_list_foreach(head, &ctx->file_config) {
        entry = mk_list_entry(head, struct flb_in_lua_file_info, _head);
        if (entry->wfd == event->wd)
        {
            tmp = entry;
            entry->new_file = MK_TRUE;
            break;
        }
    }

    if (tmp) {
        data_len += snprintf(buf,
                             4096,
                             "%s create in %s.\n",
                             event->name,
                             tmp->file_config.log_directory);
        buf[data_len] = '\0';
        file_head.len = htonl(data_len);

        data_len = data_encode(COMMAND_FILE,
                               &file_head,
                               sizeof(file_head),
                               buf,
                               data_len,
                               ctx->buf + ctx->read_len,
                               ctx->buf_len - ctx->read_len);
        if (data_len > 0) {
            ctx->read_len += data_len;
        }
    }

    return;
}

static void in_lua_file_delete(struct flb_in_lua_config *ctx, struct inotify_event *event) {
    struct flb_in_lua_file_info *entry;
    struct flb_in_lua_file_info *tmp = NULL;
    struct mk_list *head;
    char buf[4096];
    uint32_t data_len = 5;
    FILE_HEAD_REQ_S file_head;

    mk_list_foreach(head, &ctx->file_config) {
        entry = mk_list_entry(head, struct flb_in_lua_file_info, _head);
        if (entry->wfd == event->wd)
        {
            tmp = entry;
            if (0 == strcmp(entry->file_name, event->name)){
                in_lua_file_close(ctx, entry);
                break;
            }
        }
    }

    if (tmp) {
        data_len += snprintf(buf,
                             4096,
                             "%s delete from %s.\n",
                             event->name,
                             tmp->file_config.log_directory);
        buf[data_len] = '\0';
        file_head.len = htonl(data_len);

        data_len = data_encode(COMMAND_FILE,
                               &file_head,
                               sizeof(file_head),
                               buf,
                               data_len,
                               ctx->buf + ctx->read_len,
                               ctx->buf_len - ctx->read_len);
        if (data_len > 0) {
            ctx->read_len += data_len;
        }
    }

    return;
}

static void in_lua_file_modify(struct flb_in_lua_config *ctx, struct inotify_event *event) {
    struct flb_in_lua_file_info *entry;
    struct mk_list *head;
    struct stat stat;
    int len = 0;
    char buf[4096];
    FILE_HEAD_REQ_S file_head;

    mk_list_foreach(head, &ctx->file_config) {
        entry = mk_list_entry(head, struct flb_in_lua_file_info, _head);
        if (entry->wfd == event->wd && 0 == strcmp(entry->file_name, event->name) && entry->file_fd != -1)
        {
            fstat(entry->file_fd, &stat);
            if (entry->file_stat.st_uid != stat.st_uid) {
                len = snprintf(buf, 4096, "user_id change to %d", stat.st_uid);

                buf[len] = '\0';
            }

            if (entry->file_stat.st_gid != stat.st_gid) {
                len += snprintf(&buf[len], 4096 - len, " group_id change to %d", stat.st_gid);
                buf[len] = 0;
            }

            if (len  > 0) {
                len += snprintf(&buf[len], 4096 - len, " of %s/%s", entry->file_config.log_directory, entry->file_name);
                file_head.len = htonl(len);

                len = data_encode(COMMAND_FILE,
                                       &file_head,
                                       sizeof(file_head),
                                       buf,
                                       len,
                                       ctx->buf + ctx->read_len,
                                       ctx->buf_len - ctx->read_len);
                if (len > 0) {
                    ctx->read_len += len;
                }
            }
            break;
        }
    }
    return;
}

static int in_lua_file_event(struct flb_in_lua_config *ctx, int i_watch_fd)
{
    int i_read_len = -1;
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
                     case IN_MOVED_FROM:{
                         in_lua_file_move_from(ctx, pst_inotify_event);
                         break;
                     }
                     case IN_MOVED_TO: {
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
    return 0;
}

int in_lua_read_event(void *data) {
    struct mk_event *event = data;
    struct flb_in_lua_config *ctx = event->data;

    in_lua_file_event(ctx, event->fd);
    return 0;
}

int file_open_behave(struct flb_in_lua_config *ctx,
                     struct flb_in_lua_file_info *file,
                     int stream_id,
                     int sub_stream_id,
                     const char *file_name)
{
    //printf("file_open_behave.\r\n");
    int len = -1;
    COMMAND_OPEN_REQ_S stReq;
    struct stat *stat = &file->file_stat;
    fstat(file->file_fd, stat);

    stReq.create_timestamp = htonl(stat->st_ctime);
    stReq.modify_timestamp = htonl(stat->st_mtime);
    stReq.group = htonl(stat->st_gid);
    stReq.owner = htonl(stat->st_uid);
    stReq.mod = htonl(stat->st_mode);
    stReq.offset = htonl(file->offset);
    stReq.stream_id = htonl(stream_id);
    stReq.substream_id = htonl(sub_stream_id);
    stReq.tlv_len = 0;
    stReq.filename_len = htons(strlen(file_name));

    len = data_encode(COMMAND_STREAM_START,
                        &stReq,
                        sizeof(stReq),
                        (void *)file_name,
                        ntohs(stReq.filename_len),
                        ctx->buf + ctx->read_len,
                        ctx->buf_len - ctx->read_len);

    if (len > 0) {
        ctx->read_len += len;
    }

    return 0;
}


int in_lua_file_open(struct flb_in_lua_file_info *file, struct flb_in_lua_config *ctx)
{
    char path[4096];
    int len = 0;

    len = snprintf(path, 4096, "%s/%s", file->file_config.log_directory, file->file_name);
    path[len] = '\0';
    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        printf("open file %s failed.\r\n", path);
        return -1;
    }

    if (file->offset > 0)
    {
        lseek(fd, file->offset, SEEK_SET);
    }

    file->file_fd = fd;

    file_open_behave(ctx, file, fd, 0, path);

    flb_info("file = %s, fd = %d", path, fd);
    file->new_file = MK_FALSE;
    file->offset = 0;

    return fd;
}

int in_lua_file_read(struct flb_in_lua_config *ctx, struct flb_in_lua_file_info *file) {

    static int current_fd = -1;
    uint32_t real_data_len = 0;
    char *pc_current = NULL;
    lua_State *L = ctx->lua_state;
    const char *res = NULL;
    int read_len;
    DATA_HEAD_REQ_S data_head;
    struct timeval now;
    int buf_len;
    struct stat stat;

    fstat(file->file_fd, &stat);

    if (file->offset == stat.st_size){
        return 0;
    }


    buf_len = ctx->buf_len  - ctx->read_len;
    if (buf_len > g_buf_len){
        buf_len = g_buf_len;
    }


    read_len = read(file->file_fd, g_buf, buf_len);
    if (read_len > 0){
        if (current_fd != file->file_fd){
            file_stream_behave(file, ctx);
        }
        pc_current = g_buf + read_len - 1;
        for ( ; pc_current >= g_buf; --pc_current)
        {
            if (*pc_current == '\n')
            {
                real_data_len = pc_current - g_buf + 1;
                break;
            }
        }
        //暂时不考虑单条日志超1Ｍ的情况，后续追加。
        if (real_data_len < read_len  && real_data_len > 0) {
            lseek(file->file_fd,  real_data_len - read_len, SEEK_CUR);
        }

        if (real_data_len == 0) {
            real_data_len = read_len;
        }
        file->offset += real_data_len;

        lua_getglobal(L, "process");
        lua_pushlstring(L, g_buf, real_data_len);
        lua_pcall(L, 1, 2, 0);
        res = lua_tostring(L, -2);
        read_len = lua_tointeger(L, -1);
        lua_pop(L, 2);

        //todo 填充到buf
        gettimeofday(&now, NULL);
        data_head.offset = htonll(file->offset);
        data_head.time = htonll(now.tv_sec);
        data_head.len = htonl(read_len);
        read_len = data_encode(DATA_PACK,
                               &data_head,
                               sizeof(data_head),
                               (char *)res,
                               read_len,
                               ctx->buf + ctx->read_len,
                               ctx->buf_len - ctx->read_len);
        if (read_len > 0) {
            ctx->read_len += read_len;
        }

    }
    return 0;
}

void in_lua_file_init(struct flb_in_lua_config *ctx)
{
    char file_name[4096];
    int len = 0;
    int file_num = 0;

    struct flb_in_lua_file_info *file;
    struct mk_list *head;
    struct mk_event *event;

    int ret = 0;
    g_i_inotify_fd = inotify_init1(IN_NONBLOCK);

    if (g_i_inotify_fd == -1) {
        flb_utils_error_c("File init failed for inotify init.");
    }
    event = (struct mk_event *)malloc(sizeof(struct mk_event));
    event->fd           = g_i_inotify_fd;
    event->type         = MK_EVENT_CUSTOM;
    event->mask         = MK_EVENT_EMPTY;
    event->handler      = in_lua_read_event;
    event->status       = MK_EVENT_NONE;
    event->data         = (void *)ctx;

    ret = mk_event_add(ctx->evl, g_i_inotify_fd, MK_EVENT_CUSTOM, MK_EVENT_READ, event);
    if (ret == -1) {
        close(g_i_inotify_fd);
        flb_utils_error_c("file watch event add failed.");
        return;
    }


    file_name[0] = '\0';

    ctx->read_len = 0;

    mk_list_foreach(head, &ctx->file_config) {
        file = mk_list_entry(head, struct flb_in_lua_file_info, _head);
        in_lua_add_watch(ctx, file);

        if(file->new_file) {
            //与路径整合
            len = snprintf(file_name, 4096, "%s/%s", file->file_config.log_directory, file->file_name);
            file_name[len] = '\0';
            //打开文件
            in_lua_file_open(file, ctx);
        }
        file_num ++;
    }

    if (file_num > 0) {
        g_buf_len = ctx->buf_len / file_num;
        g_buf = (char *)malloc(g_buf_len);
        if (NULL == g_buf) {
            flb_utils_error_c("read buf malloc failed.");
        }
    }
    return;
}

int file_stream_begin_behave(unsigned char ucType, struct flb_in_lua_file_info *file, struct flb_in_lua_config *ctx)
{
    int iRecv = -1;
    //unsigned int uiCurrentTime = LIMIT_GetCurrentTime();
    COMMAND_STREAM_REQ_S stReq;

    stReq.stream_id = htonl(file->file_fd);
    stReq.tlv_len = 0;

    iRecv = data_encode(ucType,
                        &stReq,
                        sizeof(stReq),
                        NULL,
                        0,
                        ctx->buf + ctx->read_len,
                        ctx->buf_len - ctx->read_len);

    //LIMIT_CheckCPULimit(uiCurrentTime, LIMIT_GetCurrentTime());
    if (iRecv > 0) {
        ctx->read_len += iRecv;
    }

    return 0;
}

int file_stream_behave(struct flb_in_lua_file_info *file, struct flb_in_lua_config *ctx)
{
    return file_stream_begin_behave(COMMAND_STREAM, file, ctx);
}

void in_lua_file_rescan(struct flb_in_lua_config *ctx) {
    struct mk_list *head;
    struct flb_in_lua_file_info *entry;

    mk_list_foreach(head, &ctx->file_config){
        entry = mk_list_entry(head, struct flb_in_lua_file_info, _head);

        if (entry->new_file) {
            in_lua_get_file(ctx, entry);
            if (entry->new_file) {
                if (entry->file_fd != -1){
                    in_lua_file_close(ctx, entry);
                }

                in_lua_file_open(entry, ctx);
                //in_lua_add_event(ctx, entry);
            }
        }
    }
}
