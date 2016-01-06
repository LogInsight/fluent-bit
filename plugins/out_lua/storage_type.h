//
// Created by user on 5/1/2016.
//

#ifndef FLUENT_BIT_STORAGE_TYPE_H
#define FLUENT_BIT_STORAGE_TYPE_H


#include <stdint.h>
#include <arpa/inet.h>

#define PACK_TYPE_MASK 0x70

#ifndef __APPLE__

    unsigned long long static inline ntohll(unsigned long long val) {
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

#endif


#pragma pack(1)

    enum pack_type {
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
    };

    enum ret_status {
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

    };

    typedef uint8_t pack_type_t;

//command: connect
    struct command_connect_req_head {
        uint16_t version;
        uint32_t userid;
        uint32_t host;
        uint32_t tlv_len;
    };

    struct command_connect_res_head {
        uint16_t status;
        uint64_t sessionid;
        uint16_t stream_count;
        uint32_t tlv_len;
        //body: [streamid] [tlv]
    };

#define proto_connect_req_hton(head) { \
    head->version = htonl(head->version); \
    head->userid = htonl(head->userid); \
    head->host = htonl(head->host); \
    head->tlv_len = htonl(head->tlv_len); \
}

#define proto_connect_req_ntoh(head) { \
    head->version = ntohl(head->version); \
    head->userid = ntohl(head->userid); \
    head->tlv_len = ntohl(head->tlv_len); \
}

#define proto_connect_res_hton(head) { \
    head->status = htons(head->status); \
    head->sessionid = htonll(head->sessionid); \
    head->stream_count = htons(head->stream_count); \
    head->tlv_len = htonl(head->tlv_len); \
}

#define proto_connect_res_ntoh(head) { \
    head->status = ntohs(head->status); \
    head->sessionid = ntohll(head->sessionid); \
    head->stream_count = ntohs(head->stream_count); \
    head->tlv_len = ntohl(head->tlv_len); \
}

////////////////////////////////////////////////////


//command: close
    struct command_close_req_head {
        char reverse;
    };

    struct command_close_res_head {
        uint16_t status;
    };

#define proto_close_res_hton(head) { \
    head->status = htons(head->status); \
}

#define proto_close_res_ntoh(head) { \
    head->status = ntohs(head->status); \
}
////////////////////////////////////////////////////


//command: stream_start
    struct command_stream_start_req_head {
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

    };

    struct command_stream_start_res_head {
        uint16_t status;
    };

#define proto_stream_start_req_hton(head) { \
    head->stream_id = htonl(head->stream_id); \
    head->substream_id = htonl(head->substream_id); \
    head->create_timestamp = htonll(head->create_timestamp); \
    head->modify_timestamp = htonll(head->modify_timestamp); \
    head->owner = htonl(head->owner); \
    head->group = htonl(head->mod); \
    head->mod = htonl(head->mod); \
    head->offset = htonll(head->offset); \
    head->filename_len = htons(head->filename_len); \
    head->tlv_len = htonl(head->tlv_len); \
}

#define proto_stream_start_req_ntoh(head) { \
    head->stream_id = ntohl(head->stream_id); \
    head->substream_id = ntohl(head->substream_id); \
    head->create_timestamp = ntohll(head->create_timestamp); \
    head->modify_timestamp = ntohll(head->modify_timestamp); \
    head->owner = ntohl(head->owner); \
    head->group = ntohl(head->mod); \
    head->mod = ntohl(head->mod); \
    head->offset = ntohll(head->offset); \
    head->filename_len = ntohs(head->filename_len); \
    head->tlv_len = ntohl(head->tlv_len); \
}

#define proto_stream_start_res_hton(head) { \
    head->status = htons(head->status); \
}

#define proto_stream_start_res_ntoh(head) { \
    head->status = ntohs(head->status); \
}

////////////////////////////////////////////////////

//command: stream_end
    struct command_stream_end_req_head {
        uint32_t stream_id;
    };

    struct command_stream_end_res_head {
        uint16_t status;
        char md5[128];
    };

#define proto_stream_end_req_hton(head) { \
    head->stream_id = htonl(head->stream_id); \
}

#define proto_stream_end_req_ntoh(head) { \
    head->stream_id = ntohl(head->stream_id); \
}

#define proto_stream_end_res_hton(head) { \
    head->status = htons(head->status); \
}

#define proto_stream_end_res_ntoh(head) { \
    head->status = ntohs(head->status); \
}
////////////////////////////////////////////////////

//command: stream
    struct command_stream_req_head {
        uint32_t stream_id;
        uint32_t tlv_len;
    };

    struct command_stream_res_head {
        uint16_t status;
    };

#define proto_stream_req_hton(head) { \
    head->stream_id = htonl(head->stream_id); \
}

#define proto_stream_req_ntoh(head) { \
    head->stream_id = ntohl(head->stream_id); \
}

#define proto_stream_res_hton(head) { \
    head->status = htons(head->status); \
}

#define proto_stream_res_ntoh(head) { \
    head->status = ntohs(head->status); \
}
////////////////////////////////////////////////////


//command: substream
    struct command_substream_req_head {
        uint32_t substreamid;
        uint32_t tlv_len;
    };

    struct command_substream_res_head {
        uint16_t status;
    };

#define proto_substream_req_hton(head) { \
    head->substreamid = htonl(head->substreamid); \
}

#define proto_substream_req_ntoh(head) { \
    head->substreamid = ntohl(head->substreamid); \
}

#define proto_substream_res_hton(head) { \
    head->status = htons(head->status); \
}

#define proto_substream_res_ntoh(head) { \
    head->status = ntohs(head->status); \
}
////////////////////////////////////////////////////

//command: file
    struct command_file_req_head {
        uint16_t file_command_len;
    };

    typedef struct command_substream_res_head command_file_res_head;

#define proto_file_req_hton(head) { \
    head->file_command_len = htons(head->file_command_len); \
}

#define proto_file_req_ntoh(head) { \
    head->file_command_len = ntohs(head->file_command_len); \
}
////////////////////////////////////////////////////

//command: data_pack
    struct data_pack_req_head {
        uint32_t length;
    };

    struct data_pack_res_head {
        uint16_t status;
    };

#define proto_data_req_hton(head) { \
    head->length = htonl(head->length); \
}

#define proto_data_req_ntoh(head) { \
    head->length = ntohl(head->length); \
}

#define proto_data_res_hton(head) { \
    head->status = htons(head->status); \
}

#define proto_data_res_ntoh(head) { \
    head->status = ntohs(head->status); \
}
////////////////////////////////////////////////////


//command: stream_info
    typedef struct command_stream_info_res_head command_stream_info_req_head;
    struct command_stream_info_res_head {
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
    };

#define proto_stream_info_req_hton(head) { \
    head->status = htons(head->status); \
    head->stream_id = htonl(head->stream_id); \
    head->substream_id = htonl(head->substream_id); \
    head->create_timestamp = htonll(head->create_timestamp); \
    head->modify_timestamp = htonll(head->modify_timestamp); \
    head->owner = htonl(head->owner); \
    head->group = htonl(head->mod); \
    head->mod = htonl(head->mod); \
    head->offset = htonll(head->offset); \
    head->filename_len = htons(head->filename_len); \
    head->tlv_len = htonl(head->tlv_len); \
}

#define proto_stream_info_res_ntoh(head) { \
    head->status = ntohs(head->status); \
    head->stream_id = ntohl(head->stream_id); \
    head->substream_id = ntohl(head->substream_id); \
    head->create_timestamp = ntohll(head->create_timestamp); \
    head->modify_timestamp = ntohll(head->modify_timestamp); \
    head->owner = ntohl(head->owner); \
    head->group = ntohl(head->mod); \
    head->mod = ntohl(head->mod); \
    head->offset = ntohll(head->offset); \
    head->filename_len = ntohs(head->filename_len); \
    head->tlv_len = ntohl(head->tlv_len); \
}
////////////////////////////////////////////////////

#pragma pack()

#endif //FLUENT_BIT_STORAGE_TYPE_H
