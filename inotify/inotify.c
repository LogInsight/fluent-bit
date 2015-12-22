//
// Created by mayabin on 15/12/10.
//

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/socket.h>
#include <string.h>

#include <arpa/inet.h>

#include <netinet/in.h>

#include <lua5.1/lua.h>
#include <lua5.1/lauxlib.h>
#include <lua5.1/lualib.h>

#include "inotify.h"

#define register_constant(s)\
    lua_pushinteger(L, s);\
    lua_setfield(L, -2, #s);

#define DATA_HEAD_LEN    5

const unsigned int  g_cus_version = 1;
const unsigned int g_ui_send_buf_len = (1 << 20) + 128;

static unsigned int g_ui_data_len = 0;

static int g_i_epoll_fd = -1;
static int g_i_inotify_fd = -1;
static int g_i_timeout = 1000;
static unsigned int g_ui_buf_len = 0;
static char *g_pc_buf = NULL;
static int g_i_socket_fd = -1;
static unsigned int  g_ui_host = 0;

static int g_i_current_fd = -1;

static char *g_pc_send_buf = NULL;


int socket_send_recv(void *pSendBuf, unsigned int uiSendLen, void *pRecvBuf, unsigned int uiRecvLen)
{
    int iSend = -1;
    unsigned int uiRemain = 0;
    if (0 > g_i_socket_fd)
    {
        return -1;
    }
    //printf("send len:%d\r\n", uiSendLen);
    while(uiRemain < uiSendLen)
    {
        iSend = send(g_i_socket_fd, pSendBuf + uiRemain, uiSendLen - uiRemain, 0);
        if (iSend < 0)
        {
            return -1;
        }
        uiRemain += iSend;
    }
    return recv(g_i_socket_fd, pRecvBuf, uiRecvLen, 0);
}

int socket_connect_behave(void)
{
    //printf("socket_connect_behave.\r\n");
    char sz_buf[4096];
    int iRecv = -1;
    COMMAND_CONNECT_REQ_S stReq;
    COMMAND_CONNECT_RES_S *pstRes = NULL;

    stReq.version = htons(g_cus_version);
    stReq.host = g_ui_host;
    stReq.userid = 0;
    stReq.tlv_len = 0;

    iRecv = data_encode(COMMAND_CONNECT, &stReq, sizeof(stReq), NULL, 0, sz_buf, 4096);

    iRecv = socket_send_recv(sz_buf, iRecv, sz_buf, 4096);
    if (iRecv < 0)
    {
        printf("communication with server failed.\r\n");
        exit(0); 
    }
    if (iRecv >= sizeof(COMMAND_CONNECT_RES_S))
    {
        pstRes = (COMMAND_CONNECT_RES_S *)sz_buf;
        if (ntohs(pstRes->status) == 200)
        {
            printf("connect success.\r\n");
        }
        if(pstRes->tlv_len > 0)
        {

        }
    }
    return 0;
}

int file_open_behave(int fd, int offset, int stream_id, int sub_stream_id)
{
    //printf("file_open_behave.\r\n");
    int iRecv = -1;
    char szBuf[4096];
    COMMAND_OPEN_RES_S *pstRes = (COMMAND_OPEN_RES_S *)szBuf;
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

    iRecv = data_encode(COMMAND_STREAM_START, &stReq, sizeof(stReq), NULL, 0, szBuf, 4096);
    iRecv = socket_send_recv(szBuf, iRecv, szBuf, 4096);
    if (iRecv < 0)
    {
        //printf("open behave failed.\r\n");
        exit(0);
    }

    if (ntohs(pstRes->status) == 200)
    {
        printf("open success.\r\n");

    }

    return 0;
}
int file_stream_begin_behave(unsigned char ucType, int uiStreamId)
{
    //printf("file_stream_begin_behave. type = %d\r\n", ucType);
    int iRecv = -1;
    char szBuf[4096];
    COMMAND_STREAM_REQ_S stReq;
    COMMAND_STREAM_RES_S *pstRes = (COMMAND_STREAM_RES_S *)szBuf;

    stReq.stream_id = htonl(uiStreamId);
    stReq.tlv_len = 0;

    iRecv = data_encode(ucType, &stReq, sizeof(stReq), NULL, 0, szBuf, 4096);

    iRecv = socket_send_recv(szBuf, iRecv, szBuf, 4096);

    if (iRecv < 0)
    {
        printf("stream behave failed.\r\n");
        exit(0);
    }
    //printf("status = %d \r\n", ntohs(pstRes->status));
    if (ntohs(pstRes->status) == RET_STATUS_OK)
    {
        printf("stream behave success.\r\n");
    }
    return 0;
}

int file_stream_behave(unsigned int uiStreamId)
{
    return file_stream_begin_behave(COMMAND_STREAM, uiStreamId);
}

int file_substream_behave(unsigned int uiStreamId)
{
    return file_stream_begin_behave(COMMAND_SUBSTREAM, uiStreamId);
}

int file_close_behave(int iStreamId)
{
    //printf("file_close_behave.\r\n");
    int iRecv = -1;
    char szBuf[4096];
    COMMAND_STREAM_END_REQ_S stReq;
    COMMAND_STREAM_END_RES_S *pstRes;

    stReq.stream_id = htonl(iStreamId);
    iRecv = data_encode(COMMAND_STREAM_END, &stReq, sizeof(stReq), NULL, 0, szBuf, 4096);

    iRecv = socket_send_recv(szBuf, iRecv, szBuf, 4096);
    if (iRecv < 0)
    {
        printf("file close behave failed.\r\n");
        exit(0);
    }

    pstRes = (COMMAND_STREAM_END_RES_S *)szBuf;

    if (ntohs(pstRes->status) == RET_STATUS_OK)
    {
        printf("%s\r\n", pstRes->md5);
    }
    return 0;
}


int socket_close_behave()
{
    //printf("scoket_close_behave.\r\n");
    int iRecv = -1;
    char sz_buf[4096];
    COMMAND_CLOSE_REQ_S stReq;
    COMMAND_CONNECT_RES_S *pstRes = NULL;
    stReq.reverse = COMMAND_CLOSE;

    iRecv = data_encode(COMMAND_CLOSE, &stReq, sizeof(stReq), NULL, 0, sz_buf, 4096);
    iRecv = socket_send_recv(sz_buf, iRecv, sz_buf, 4096);
    if (iRecv < 0)
    {
        printf("close failed.\r\n");
        exit(0);
    }
    pstRes = (COMMAND_CONNECT_RES_S *)sz_buf;

    if (ntohs(pstRes->status) == 200)
    {
        printf("close success.\r\n");
    }

    return 0;
}



void socket_create(const char *cpc_ip, unsigned short us_port)
{
    int socket_fd = -1;
    char sz_buf[128];
    struct sockaddr_in st_addr_in;
    memset(&st_addr_in, 0, sizeof(st_addr_in));
    socket_fd = socket(AF_INET,SOCK_STREAM,0);
    if (socket_fd < 0)
    {
        printf("socket create error.\r\n");
        return;
    }

    st_addr_in.sin_addr.s_addr = inet_addr(cpc_ip);
    st_addr_in.sin_family = AF_INET;
    st_addr_in.sin_port = htons(us_port);

    if (0 >  connect(socket_fd, (struct sockaddr *)&st_addr_in, sizeof(st_addr_in)))
    {
        close(socket_fd);
        printf("connect error.\r\n");
        return;
    }
    g_i_socket_fd = socket_fd;
    recv(g_i_socket_fd, sz_buf, 128, 0);
    return;
}

int file_init(lua_State *L)
{

    unsigned short us_port = 0;
    int i_timeout = -1;
    unsigned int ui_buf_len = 0;
    const char *cpc_ip = NULL;
    struct epoll_event st_epoll_event;

    i_timeout = luaL_checknumber(L, 1);
    ui_buf_len = luaL_checknumber(L, 2);
    cpc_ip = luaL_checkstring(L,3);
    us_port = luaL_checknumber(L, 4);
    g_ui_host = inet_addr(luaL_checkstring(L, 5));

    assert(ui_buf_len > 0 && ui_buf_len <= 64);
    if (i_timeout >= 0)
    {
        g_i_timeout *= i_timeout;
    }
    else
    {
        g_i_timeout = -1;
    }

    g_ui_buf_len = ui_buf_len << 20;
    g_pc_buf = (char *)malloc(g_ui_buf_len);
    if (NULL == g_pc_buf)
    {
        printf("get memory buf failed.\r\n");
        exit(0);
    }

    g_pc_send_buf = (char *)malloc(g_ui_send_buf_len);
    if(NULL == g_pc_send_buf)
    {
        printf("send buf create failed.\r\n");
        exit(0);
    }

    socket_create(cpc_ip, us_port);
    if (g_i_socket_fd == -1)
    {
        exit(0);
    }
    socket_connect_behave();

    g_i_epoll_fd = epoll_create(1024);
    g_i_inotify_fd = inotify_init1(IN_NONBLOCK);
    if (g_i_epoll_fd == -1 || g_i_inotify_fd == -1)
    {
        printf("file init failed.\r\n");
        if (-1 != g_i_epoll_fd)
        {
            close(g_i_epoll_fd);
            g_i_epoll_fd = -1;
        }
        if (-1 != g_i_inotify_fd)
        {
            close(g_i_inotify_fd);
            g_i_inotify_fd = -1;
        }
        close(g_i_socket_fd);
        free(g_pc_buf);
        exit(0);
    }

    st_epoll_event.events = EPOLLIN|EPOLLHUP;
    st_epoll_event.data.fd = g_i_inotify_fd;
    epoll_ctl(g_i_epoll_fd, EPOLL_CTL_ADD, g_i_inotify_fd, &st_epoll_event);
    g_ui_data_len = DATA_HEAD_LEN;
    return 0;
}

int file_data_save(lua_State *L)
{
    const char *cpc_data = luaL_checkstring(L, 1);
    int iReadLen = -1;
    unsigned int ui_data_len = luaL_checknumber(L, 2);
    unsigned int *puiLen;
    DATA_HEAD_RES_S *pstRes;


    if ((g_ui_data_len + ui_data_len) > g_ui_send_buf_len)
    {
        puiLen = (unsigned int *)g_pc_send_buf;
        *puiLen = htonl(g_ui_data_len - sizeof(int));
        ++ puiLen;
        *((unsigned char *)puiLen) = DATA_PACK;
        pstRes = (DATA_HEAD_RES_S *)g_pc_send_buf;

        iReadLen = socket_send_recv(g_pc_send_buf, g_ui_data_len, g_pc_send_buf, g_ui_send_buf_len);
        if (iReadLen < 0)
        {
            printf("send and recv error.\r\n");
        }
        else if (ntohs(pstRes->usStatus) != RET_STATUS_OK)
        {
            printf("cache error code %d.\r\n", ntohs(pstRes->usStatus));
        }

        g_ui_data_len = DATA_HEAD_LEN;
    }

    memcpy(g_pc_send_buf + g_ui_data_len, cpc_data, ui_data_len);
    g_ui_data_len += ui_data_len;
    return 0;
}

int file_data_send(lua_State *L)
{
    //DATA_HEAD_REQ_S stReq;
    int i_read_len = 0;
    unsigned int *puiTmp = (unsigned int *)g_pc_send_buf;
    DATA_HEAD_RES_S *pstRes = (DATA_HEAD_RES_S *)g_pc_send_buf;

    if (g_ui_data_len > DATA_HEAD_LEN)
    {
        *puiTmp = (unsigned int)htonl(g_ui_data_len - sizeof(int));
        ++puiTmp;
        *((unsigned char *) puiTmp) = DATA_PACK;

        i_read_len = socket_send_recv(g_pc_send_buf, g_ui_data_len, g_pc_send_buf, g_ui_send_buf_len);
        if (i_read_len < 0)
        {
            printf("data send error.\r\n");
        }
        else if (ntohs(pstRes->usStatus) != RET_STATUS_OK)
        {
            printf("error code: %d", ntohs(pstRes->usStatus));
        }

        g_ui_data_len = DATA_HEAD_LEN;
    }

    return 0;
}

int file_add_watch_path(lua_State *L)
{
    int i_watch_fd = -1;
    const char *cpc_path = luaL_checkstring(L, 1);
    i_watch_fd = inotify_add_watch(g_i_inotify_fd, cpc_path, IN_ALL_EVENTS);
    if (i_watch_fd < 0)
    {
        lua_pushnil(L);
        printf("watch path %s failed.\r\n", cpc_path);
        return 1;
    }

    lua_pushinteger(L, i_watch_fd);
    return 1;
}



int file_open(lua_State *L)
{
    unsigned int ui_seek_pos = 0;
    const char *cpc_path = luaL_checkstring(L, 1);

    assert(NULL != cpc_path);
    ui_seek_pos = luaL_checknumber(L, 2);

    int fd = open(cpc_path, O_RDONLY);
    if (fd < 0)
    {
        printf("open file %s failed.\r\n", cpc_path);
        lua_pushnil(L);
        return 1;
    }

    if (ui_seek_pos > 0)
    {
        lseek(fd, ui_seek_pos, SEEK_SET);
    }
    else if (ui_seek_pos == -1)
    {
        ui_seek_pos = lseek(fd, 0, SEEK_END);
    }

    file_open_behave(fd, ui_seek_pos, fd, 0);


    //send file info

	lua_pushinteger(L, fd);
	return 1;
}

int file_close(lua_State *L)
{
    int i_fd = luaL_checknumber(L, 1);
    file_close_behave(i_fd);
    if (g_i_current_fd == i_fd)
    {
        g_i_current_fd = -1;
    }
    close(i_fd);
    return 0;
}

int file_rm_watch_fd(lua_State *L)
{
    int i_watch_fd = luaL_checknumber(L, 1);
    inotify_rm_watch(g_i_inotify_fd, i_watch_fd);
    return 0;
}

static int file_data_set_table(lua_State *L, struct inotify_event *pst_inotify_event)
{
    if (pst_inotify_event->len <= 0)
    {
        return -1;
    }
    lua_createtable(L, 0, 4);

    lua_pushinteger(L, pst_inotify_event->wd);
    lua_setfield(L, -2, "wd");

    lua_pushinteger(L, pst_inotify_event->mask);
    lua_setfield(L, -2, "mask");

    lua_pushinteger(L, pst_inotify_event->cookie);
    lua_setfield(L, -2, "cookie");

    lua_pushstring(L, pst_inotify_event->name);//, pst_inotify_event->len);
    lua_setfield(L, -2, "name");

    return 0;
}

static int file_data_get(lua_State *L, int i_watch_fd)
{
    int i_read_len = -1;
    int i_num = 0;
    char sz_buf[4096];
    sz_buf[0] = '\0';

    struct inotify_event *pst_inotify_event = NULL;
    int i_offset = 0;
    lua_newtable(L);
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
            if (file_data_set_table(L, pst_inotify_event) == 0)
                lua_rawseti(L, -2, ++i_num);

            i_offset += sizeof(struct inotify_event) + pst_inotify_event->len;
        }
    }
    return 1;

}

int file_event_read(lua_State *L)
{
    int i_event_num = -1;
    struct epoll_event st_epoll_event;
    //memset(&st_epoll_event, 0, sizeof(st_epoll_event));

    i_event_num = epoll_wait(g_i_epoll_fd, &st_epoll_event, 1, g_i_timeout);

    if (i_event_num > 0)
    {
         return file_data_get(L, st_epoll_event.data.fd);
    }
    if (i_event_num == 0)
    {
        lua_pushnil(L);
        return 1;
    }
}

int file_deinit(lua_State *L)
{
	epoll_ctl(g_i_epoll_fd, EPOLL_CTL_DEL, g_i_inotify_fd, NULL);
    close(g_i_inotify_fd);
    close(g_i_epoll_fd);
    free(g_pc_buf);
    g_pc_buf = NULL;
    g_ui_buf_len = 0;
    socket_close_behave();
    close(g_i_socket_fd);
    exit(0);
}

int file_data_read(lua_State *L)
{
    int i_fd = luaL_checknumber(L, 1);
    int i_read_len = -1;
    int i_key_num = 1;
    char *pc_current = g_pc_buf;
    char *pc_start = g_pc_buf;
    lua_newtable(L);
    i_read_len = read(i_fd, g_pc_buf, g_ui_buf_len);
    if (i_read_len > 0)
    {
        for ( ; pc_current < g_pc_buf + i_read_len; ++pc_current)
        {
            if (*pc_current == '\n')
            {
                lua_pushlstring(L, pc_start, pc_current - pc_start + 1);
                lua_rawseti(L, -2, i_key_num ++);
                pc_start = pc_current + 1;
            }
        }

        if (pc_current != pc_start)
        {
            if (i_key_num > 1)
            {
                lseek(i_fd, pc_start - pc_current, SEEK_CUR);
            }
            else
            {
                lua_pushlstring(L, pc_start, i_read_len);
                lua_rawseti(L, -2, i_key_num);
            }
        }
    }
    else
    {
        return 0;
    }

    if (g_i_current_fd != i_fd)
    {
        file_stream_behave(i_fd);
        g_i_current_fd = i_fd;
    }
    return 1;
}

static luaL_Reg mylibs[] = {
    {"init", file_init},
    {"add_watch", file_add_watch_path},
    {"open", file_open},
    {"close", file_close},
    {"rm_watch", file_rm_watch_fd},
    {"read_event", file_event_read},
    {"deinit", file_deinit},
    {"read_lines", file_data_read},
    {"send", file_data_send},
    {"cache", file_data_save},
    {NULL, NULL}
};



int luaopen_inotify(lua_State *L)
{
    luaL_register(L, "inotify", mylibs);
    register_constant(IN_ACCESS);
    register_constant(IN_ATTRIB);
    register_constant(IN_CLOSE_WRITE);
    register_constant(IN_CLOSE_NOWRITE);
    register_constant(IN_CREATE);
    register_constant(IN_DELETE);
    register_constant(IN_DELETE_SELF);
    register_constant(IN_MODIFY);
    register_constant(IN_MOVE_SELF);
    register_constant(IN_MOVED_FROM);
    register_constant(IN_MOVED_TO);
    register_constant(IN_OPEN);
    register_constant(IN_ALL_EVENTS);
    register_constant(IN_MOVE);
    register_constant(IN_CLOSE);
    register_constant(IN_DONT_FOLLOW);
    register_constant(IN_MASK_ADD);
    register_constant(IN_ONESHOT);
    register_constant(IN_ONLYDIR);
    register_constant(IN_IGNORED);
    register_constant(IN_ISDIR);
    register_constant(IN_Q_OVERFLOW);
    register_constant(IN_UNMOUNT);
    return 1;
}



