//
// Created by mayabin on 15/12/30.
//

#ifndef FLUENT_BIT_IN_LUA_FILE_H
#define FLUENT_BIT_IN_LUA_FILE_H

#pragma pack(1)

typedef enum pack_type {
    DATA_PACK = 0,               // 数据包
    COMMAND_CONNECT = 1,         // 与服务器获取连接, 新建或恢复session
    COMMAND_CLOSE = 2,           // 关闭与服务器的连接及关闭session
    COMMAND_STREAM_START = 3,    // 表示流开始
    COMMAND_STREAM_END = 4,      // 表示流结束
    COMMAND_STREAM = 5,          // 切换流,表示后面将要发送该流的数据包
    COMMAND_SUBSTREAM = 6,       // 切换子流,表示后面将要发送该子流的数据包
    COMMAND_FILE = 7,            // 表示文件的一系列操作(移动/复制/删除/改名等)
    COMMAND_STREAM_INFO = 8,     // 获取stream的详细信息
    COMMAND_KEEP_ALIVE = 9,      // 保活命令
    ERROR_COMMAND
}PACK_TYPE_E;

typedef enum ret_status {
    RET_STATUS_OK = 200,                       // ok
    RET_STATUS_AUTH_FAILED = 403,              // 服务器认证失败
    RET_STATUS_PACK_TYPE_ERROR = 501,          // 包类型错误
    RET_STATUS_COMMAND_ERROR = 502,            // 命令错误, 服务器此时不接受该命令
    RET_STATUS_COMMAND_DATA_ERROR = 503,       // 报文解析失败
    RET_STATUS_STREAM_HAS_NOT_CLOSE = 504,     // 执行close时, 存在stream没有关闭
    RET_STATUS_WRITE_DATA_ERROR = 505,         // 数据包处理失败
    RET_STATUS_COMMAND_STREAMID_ERROR = 506,   // 执行stream命令时, streamid不存在
    RET_STATUS_COMMAND_STREAM_EXIST = 507,     // 执行stream_start时,stream已经存在
    RET_STATUS_TOO_MUCH_SESSION = 508,         // 超过最大session数
    RET_STATUS_TOO_MUCH_STREAM = 509,          // 超过最大Stream数
    RET_STATUS_SERVER_BUSY = 510               // 超过最大报文缓冲队列

}RET_STATUS_E;

typedef struct command_connect_req_head {
    uint16_t version;
    uint32_t userid;
    uint32_t host;
    uint32_t tlv_len;
}COMMAND_CONNECT_REQ_S;

typedef struct command_connect_res_head {
    uint16_t status;
    uint64_t sessionid;
    uint16_t stream_count;
    uint32_t tlv_len;
}COMMAND_CONNECT_RES_S;

typedef struct command_close_req_head {
    char reverse;
}COMMAND_CLOSE_REQ_S;

typedef struct command_close_res_head {
    uint16_t status;
}COMMAND_CLOSE_RES_S;

//command: stream_start
typedef struct command_stream_start_req_head {
    uint32_t stream_id;
    uint32_t substream_id;
    uint64_t create_timestamp;
    uint64_t modify_timestamp;
    uint32_t owner;
    uint32_t group;
    uint32_t mod;
    uint64_t offset;
    uint16_t filename_len;
    uint32_t tlv_len;
}COMMAND_OPEN_REQ_S;

typedef struct command_stream_start_res_head {
    uint16_t status;
}COMMAND_OPEN_RES_S;


//command: stream_end
typedef struct command_stream_end_req_head {
    uint32_t stream_id;
}COMMAND_STREAM_END_REQ_S;

typedef struct command_stream_end_res_head {
    uint16_t status;
    char md5[128];
}COMMAND_STREAM_END_RES_S;

typedef struct command_stream_req_head {
    uint32_t stream_id;
    uint32_t tlv_len;
}COMMAND_STREAM_REQ_S;

typedef struct command_stream_res_head {
    uint16_t status;
}COMMAND_STREAM_RES_S;

typedef struct tag_data_head_s
{
    unsigned int len;
    uint64_t offset;
    uint64_t time;
}DATA_HEAD_REQ_S;

typedef struct tag_data_head_res_s
{
    unsigned short status;
}DATA_HEAD_RES_S;


typedef struct tag_file_head_req_s {
    uint32_t len;
}FILE_HEAD_REQ_S;


typedef struct tag_tl_head_s
{
    unsigned int type;
    unsigned int len;
}TL_HEAD_S;

typedef struct tag_tlv_head_s
{
    TL_HEAD_S stTl;
    char *pcValue;
}TLV_HEAD_S;

typedef COMMAND_STREAM_REQ_S COMMAND_STREAM_INFO_REQ_S;

typedef struct command_stream_info_res_head {
    uint16_t status;
    uint32_t stream_id;
    uint32_t substream_id;
    uint64_t create_timestamp;
    uint64_t modify_timestamp;
    uint32_t owner;
    uint32_t group;
    uint32_t mod;
    uint64_t offset;
    uint16_t filename_len;
    uint32_t tlv_len;
    //body: [filename] [tlv]
}COMMAND_STREAM_INFO_RES_S;


typedef struct tag_stream_id_info_s
{
    uint16_t stream_num;
    uint32_t *stream_id;
}STREAM_ID_INFO_S;

#pragma pack()

int tlv_encode(TLV_HEAD_S *pstHead, void *pBuf, unsigned int uiBufLen);

int data_encode(unsigned char ucType,
                void *pHead,
                unsigned  int uiHeadLen,
                void *pBuf,
                unsigned int uiBufLen,
                void *pOutBuf,
                unsigned int uiOutBufLen);

void in_lua_file_init(struct flb_in_lua_config *ctx);
void in_lua_file_rescan(struct flb_in_lua_config *ctx);


unsigned long long static inline ntohll(unsigned long long val)
{
    if (__BYTE_ORDER == __LITTLE_ENDIAN)
    {
        return (((unsigned long long )htonl((int)((val << 32) >> 32))) << 32) | (unsigned int)htonl((int)(val >> 32));
    }
    else if (__BYTE_ORDER == __BIG_ENDIAN)
    {
        return val;
    }
    return 0;
}

unsigned long long static inline htonll(unsigned long long val)
{
    if (__BYTE_ORDER == __LITTLE_ENDIAN)
    {
        return (((unsigned long long )htonl((int)((val << 32) >> 32))) << 32) | (unsigned int)htonl((int)(val >> 32));
    }
    else if (__BYTE_ORDER == __BIG_ENDIAN)
    {
        return val;
    }
}


#endif //FLUENT_BIT_IN_LUA_FILE_H
