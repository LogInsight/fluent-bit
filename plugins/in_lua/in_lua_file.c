//
// Created by mayabin on 15/12/30.
//

//#include <sys/timerfd.h>
//#include <time.h>

#include <fluent-bit/flb_utils.h>
#include <sys/inotify.h>

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

//typedef void (*file_event_callback) (struct flb_in_lua_config, struct flb_in_lua_file_info);

int tlv_encode(TLV_HEAD_S *pstHead, void *pBuf, unsigned int uiBufLen)
{
    unsigned int uiLen = sizeof(pstHead->stTl) + pstHead->stTl.uiLen;

    if(uiLen > uiBufLen)
    {
        return -1;
    }
    memcpy(pBuf, pstHead, sizeof(pstHead->stTl));
    memcpy(pBuf + sizeof(pstHead->stTl), pstHead->pcValue, pstHead->stTl.uiLen);
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

//move in
static void in_lua_file_move_to(struct flb_in_lua_config *ctx, struct inotify_event *event) {
    struct flb_in_lua_file_info *entry;
    struct flb_in_lua_file_info *tmp = NULL;
    struct mk_list *head;
    char buf[4096];
    uint32_t *len = (uint32_t *)buf;
    int data_len = 0;

    mk_list_foreach(head, &ctx->file_config) {
        entry = mk_list_entry(head, struct flb_in_lua_file_info, _head);
        if (entry->wfd == event->wd)
        {
            entry->new_file = MK_TRUE;
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
        //in_lua_data_write(buf, data_len);
        //todo write data to buf
    }
    return;
}

//move out
static void in_lua_file_move_from(struct flb_in_lua_config *ctx, struct inotify_event *event) {
    struct flb_in_lua_file_info *entry;
    struct flb_in_lua_file_info *tmp = NULL;
    struct mk_list *head;
    char buf[4096];
    uint32_t *len = (uint32_t *)buf;
    uint32_t data_len = 5;

    mk_list_foreach(head, &ctx->file_config) {
        entry = mk_list_entry(head, struct flb_in_lua_file_info, _head);
        if (entry->wfd == event->wd)
        {
            tmp = entry;
            if (entry->file_fd != -1  && 0 == strcmp(entry->file_name, event->name))
            {
                close(entry->file_fd);
                entry->file_fd = -1;
                entry->new_file = MK_TRUE;
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
        //in_lua_data_write(buf, data_len);
    }
    return;
}

static void in_lua_file_create(struct flb_in_lua_config *ctx, struct inotify_event *event) {
    struct flb_in_lua_file_info *entry;
    struct flb_in_lua_file_info *tmp = NULL;
    struct mk_list *head;
    char buf[4096];
    uint32_t data_len = 5;
    uint32_t *len = (uint32_t *)buf;

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
        data_len += snprintf(&buf[5],
                             4090,
                             "%s create in %s.\n",
                             event->name,
                             tmp->file_config.log_directory);
        buf[data_len] = '\0';
        *len = htonl(data_len - 4);
        *(char *)(len + 1) = COMMAND_FILE;
        //todo write to buf
    }

    return;
}

static void in_lua_file_delete(struct flb_in_lua_config *ctx, struct inotify_event *event) {
    struct flb_in_lua_file_info *entry;
    struct flb_in_lua_file_info *tmp = NULL;
    struct mk_list *head;
    char buf[4096];
    uint32_t data_len = 5;
    uint32_t *len = (uint32_t *)buf;

    mk_list_foreach(head, &ctx->file_config) {
        entry = mk_list_entry(head, struct flb_in_lua_file_info, _head);
        if (entry->wfd == event->wd)
        {
            tmp = entry;
            if (0 == strcmp(entry->file_name, event->name) && entry->file_fd != -1){

                close(entry->file_fd);
                entry->file_fd = -1;
                entry->new_file = MK_TRUE;
                mk_event_del(ctx->evl, &entry->event);
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
        //in_lua_data_write(buf, data_len);
    }

    return;
}

static void in_lua_file_modify(struct flb_in_lua_config *ctx, struct inotify_event *event) {
    struct flb_in_lua_file_info *entry;
    struct mk_list *head;

    mk_list_foreach(head, &ctx->file_config) {
        entry = mk_list_entry(head, struct flb_in_lua_file_info, _head);
        if (entry->wfd == event->wd && 0 == strcmp(entry->file_name, event->name) && entry->file_fd != -1)
        {
            entry->changed = MK_TRUE;
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

int in_lua_file_read(void *data) {
    struct flb_in_lua_file_info *file = data;
    struct flb_in_lua_config *ctx = file->event.data;

    struct mk_list *head;
    static int current_fd = -1;
    uint32_t real_data_len = 0;
    char *pc_current = NULL;
    lua_State *L = ctx->lua_state;
    const char *res = NULL;
    static char type = DATA_PACK;
    int read_len;
    char buf[8192];
    read_len = ctx->buf_len  - ctx->read_len;
    if (read_len > 8192){
        read_len = 8192;
    }


    read_len = read(file->file_fd, buf, read_len);
    if (read_len > 0){
        if (current_fd != file->file_fd){
            file_stream_behave(file->file_fd);
        }
        pc_current = buf + read_len - 1;
        for ( ; pc_current >= buf; --pc_current)
        {
            if (*pc_current == '\n')
            {
                real_data_len = pc_current - buf + 1;
                break;
            }
        }
        //暂时不考虑单条日志超1Ｍ的情况，后续追加。
        if (real_data_len < read_len  && real_data_len > 0) {
            lseek(file->file_fd,  real_data_len - read_len, SEEK_CUR);
        }

        if (real_data_len == 0) {
            file->offset += read_len;
        }
        else {
            file->offset += real_data_len;
        }

        lua_getglobal(L, "process");
        lua_pushlstring(L, buf, real_data_len);
        lua_pcall(L, 1, 1, 0);
        res = lua_tostring(L, -1);

        //todo 填充到buf

    }
    else {
        file->changed = false;
    }
    return 0;
}

void in_lua_add_event(struct flb_in_lua_config *ctx, struct flb_in_lua_file_info *file) {
    struct mk_event *event;

    if (file->file_fd == -1) {
        return;
    }

    file->changed = MK_TRUE;

    event = &file->event;
    event->fd           = file->file_fd;
    event->type         = MK_EVENT_CUSTOM;
    event->mask         = MK_EVENT_EMPTY;
    event->handler      = in_lua_file_read;
    event->status       = MK_EVENT_NONE;
    event->data         = (void *)ctx;
    return;
}

void in_lua_file_init(struct flb_in_lua_config *ctx)
{
    char file_name[4096];
    int len = 0;

    struct flb_in_lua_file_info *file;

    struct mk_list *head;
    int file_fd = -1;

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
            //TODO:获取对应的meta信息
            file->offset = 0;
            //打开文件
            file_fd = in_lua_file_open(file_name, file->offset);

            file->file_fd = file_fd;

            in_lua_add_event(ctx, file);
            mk_event_add(ctx->evl, file_fd, MK_EVENT_CUSTOM, MK_EVENT_READ, &file);
        }
    }
}

int file_stream_begin_behave(unsigned char ucType, struct flb_in_lua_file_info *file)
{
    //printf("file_stream_begin_behave. type = %d\r\n", ucType);
    int iRecv = -1;
    //unsigned int uiCurrentTime = LIMIT_GetCurrentTime();
    char szBuf[4096];
    COMMAND_STREAM_REQ_S stReq;

    stReq.stream_id = htonl(file->file_fd);
    stReq.tlv_len = 0;

    iRecv = data_encode(ucType, &stReq, sizeof(stReq), NULL, 0, szBuf, 4096);

    //LIMIT_CheckCPULimit(uiCurrentTime, LIMIT_GetCurrentTime());
    //todo 写buf;

    return 0;
}

int file_stream_behave(struct flb_in_lua_file_info *file)
{
    return file_stream_begin_behave(COMMAND_STREAM, file);
}

void in_lua_file_rescan(struct flb_in_lua_config *ctx) {
    struct mk_list *head;
    struct flb_in_lua_file_info *entry;
    int fd = -1;

    mk_list_foreach(head, &ctx->file_config){
        entry = mk_list_entry(head, struct flb_in_lua_file_info, _head);

        if (entry->new_file) {
            entry->new_file = MK_FALSE;
            in_lua_get_file(ctx, entry);
            if (entry->new_file) {
                if (entry->file_fd != -1){
                    close(entry->file_fd);
                    entry->file_fd = -1;
                    mk_event_del(ctx->evl, &entry->event);
                }
                entry->offset = 0;

                fd = in_lua_file_open(entry->file_name, 0);

                entry->file_fd = fd;
                in_lua_add_event(ctx, entry);
            }
        }
    }
}

/*
void in_lua_file_read_time(struct flb_in_lua_config *ctx, u_int64_t times) {
    struct mk_list *head;
    struct flb_in_lua_file_info *file;
    static int current_fd = -1;
    uint32_t real_data_len = 0;
    char *pc_current = NULL;
    lua_State *L = ctx->lua_state;
    const char *res = NULL;
    static char type = DATA_PACK;
    int read_len;

    mk_list_foreach(head, &ctx->file_config) {
        file = mk_list_entry(head, struct flb_in_lua_file_info, _head);

        if(file->changed && file->file_fd != -1 && 0 == times % file->file_config.rescan_interval) {
            read_len = read(file->file_fd, ctx->buf, ctx->buf_len);
            if (read_len > 0){
                if (current_fd != file->file_fd){
                    file_stream_behave(file->file_fd);
                }
                pc_current = ctx->buf + read_len - 1;
                for ( ; pc_current >= ctx->buf; --pc_current)
                {
                    if (*pc_current == '\n')
                    {
                        real_data_len = pc_current - ctx->buf + 1;
                        break;
                    }
                }
                //暂时不考虑单条日志超1Ｍ的情况，后续追加。
                if (real_data_len < read_len  && real_data_len > 0) {
                    lseek(file->file_fd,  real_data_len - read_len, SEEK_CUR);
                }

                if (real_data_len == 0) {
                     file->offset += read_len;
                }
                else {
                    file->offset += real_data_len;
                }

                lua_getglobal(L, "process");
                lua_pushlstring(L, ctx->buf, real_data_len);
                lua_pcall(L, 1, 1, 0);
                res = lua_tostring(L, -1);

                read_len = strlen(res);
                real_data_len = htonl(read_len);
                in_lua_data_write(&real_data_len, 4);
                in_lua_data_write(&type, 1);
                in_lua_data_write((void *)res, read_len);
            }
            else {
                file->changed = false;
            }
        }
    }
    return;
}

*/
