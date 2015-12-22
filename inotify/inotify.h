#ifndef __INOTIFY_SELF_H__
#define __INOTIFY_SELF_H__

#pragma pack(1)

typedef enum tag_pack_type_e {
    DATA_PACK = 0,
    COMMAND_CONNECT  = 1,
    COMMAND_CLOSE = 2,
    COMMAND_STREAM_START = 3,
    COMMAND_STREAM_END = 4,
    COMMAND_STREAM = 5,
    COMMAND_SUBSTREAM = 6,
    COMMAND_FILE = 7,
    ERROR_COMMAND
}PACK_TYPE_E;

enum ret_status {
    RET_STATUS_OK = 200,
    RET_STATUS_PACK_TYPE_ERROR = 501,
    RET_STATUS_COMMAND_ERROR = 502,
    RET_STATUS_COMMAND_DATA_ERROR = 503,
    RET_STATUS_STREAM_HAS_NOT_CLOSE = 504,
    RET_STATUS_WRITE_DATA_ERROR = 505,

};

typedef struct command_connect_req_head {
    uint16_t version;
    uint32_t userid;
    uint32_t host;
    uint32_t tlv_len;
}COMMAND_CONNECT_REQ_S;

typedef struct command_connect_res_head {
    uint16_t status;
    uint64_t sessionid;
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
    unsigned int uiDataLen;
}DATA_HEAD_REQ_S;

typedef struct tag_data_head_res_s
{
    unsigned short usStatus;
}DATA_HEAD_RES_S;


typedef struct tag_tl_head_s
{
    unsigned int uiType;
    unsigned int uiLen;
}TL_HEAD_S;

typedef struct tag_tlv_head_s
{
    TL_HEAD_S stTl;
    char *pcValue;
}TLV_HEAD_S;

#pragma pack()

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

#endif
