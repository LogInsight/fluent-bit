//
// Created by jc on 5/1/2016.
//

#include "storage_type.h"
#include "string.h"
#include "stdbool.h"

#ifndef FLUENT_BIT_STORAGE_COMMAND_H
#define FLUENT_BIT_STORAGE_COMMAND_H

bool inline pack_command_connect(struct command_connect_req_head *connect_head, char* data, size_t len, size_t *res_len) {
    size_t ret_len = 0;
    pack_type_t type = COMMAND_CONNECT;

    if (ret_len + sizeof(type) > len) {
        return false;
    }
    *(pack_type_t *) data = type;
    ret_len += sizeof(type);

    if (ret_len + sizeof(*connect_head) > len) {
        return false;
    }
    proto_connect_req_hton(connect_head);
    *(struct command_connect_req_head *) (data + ret_len) = *connect_head;
    ret_len += sizeof(*connect_head);

    *res_len = ret_len;
    return true;
}

bool inline pack_command_close(char* data, size_t len, size_t *res_len) {
    size_t ret_len = 0;
    pack_type_t type = COMMAND_CLOSE;

    if (ret_len + sizeof(type) > len) {
        return false;
    }

    *(pack_type_t *) data = type;
    ret_len += sizeof(type);

    *res_len = ret_len;
    return true;
}

bool inline pack_command_stream_start(struct command_stream_start_req_head *head,
                                      const char *filename, size_t name_len, char* data, size_t len, size_t *res_len) {
    size_t ret_len = 0;
    pack_type_t type = COMMAND_STREAM_START;

    if (ret_len + sizeof(type) > len) {
        return false;
    }

    *(pack_type_t *) data = type;
    ret_len += sizeof(type);

    proto_stream_start_req_hton(head);
    if (ret_len + sizeof(*head) > len) {
        return false;
    }

    *(struct command_stream_start_req_head *) (data + ret_len) = *head;
    ret_len += sizeof(*head);

    memcpy(data + ret_len, filename, name_len);
    ret_len += name_len;

    *res_len = ret_len;
    return true;
}

bool inline pack_command_stream_end(struct command_stream_end_req_head *head,
                                    char* data, size_t len, size_t *res_len) {
    size_t ret_len = 0;
    pack_type_t type = COMMAND_STREAM_END;

    if (ret_len + sizeof(type) > len) {
        return false;
    }

    *(pack_type_t *) data = type;
    ret_len += sizeof(type);

    proto_stream_end_req_hton(head);

    if (ret_len + sizeof(*head) > len) {
        return false;
    }

    *(struct command_stream_end_req_head *) (data + ret_len) = *head;
    ret_len += sizeof(*head);

    *res_len = ret_len;
    return true;
};

bool inline pack_command_stream(struct command_stream_req_head *head,
                                char* data, size_t len, size_t *res_len) {
    size_t ret_len = 0;
    pack_type_t type = COMMAND_STREAM;

    if (ret_len + sizeof(type) > len) {
        return false;
    }

    *(pack_type_t *) data = type;
    ret_len += sizeof(type);

    proto_stream_req_hton(head);
    if (ret_len + sizeof(*head) > len) {
        return false;
    }

    *(struct command_stream_req_head *) (data + ret_len) = *head;
    ret_len += sizeof(*head);

    *res_len = ret_len;
    return true;
}

bool inline pack_command_substream(struct command_substream_req_head *head,
                                   char* data, size_t len, size_t *res_len) {
    size_t ret_len = 0;
    pack_type_t  type = COMMAND_SUBSTREAM;

    if (ret_len + sizeof(type) > len) {
        return false;
    }

    *(pack_type_t *) data = type;
    ret_len += sizeof(type);

    proto_substream_req_hton(head);
    if (ret_len + sizeof(*head) > len) {
        return false;
    }

    *(struct command_substream_req_head *) (data + ret_len) = *head;
    ret_len += sizeof(*head);

    *res_len = ret_len;
    return true;
}

bool inline pack_command_file(const char *command_line, size_t line_size,
                              char* data, size_t len, size_t *res_len) {
    size_t ret_len = 0;
    pack_type_t  type = COMMAND_FILE;

    if (ret_len + sizeof(type) > len) {
        return false;
    }

    *(pack_type_t *) data = type;
    ret_len += sizeof(type);

    struct command_file_req_head head;
    head.file_command_len = (uint16_t) line_size;
    proto_file_req_hton(&head);

    if (ret_len + sizeof(head) > len) {
        return false;
    }

    *(struct command_file_req_head *) (data + ret_len) = head;
    ret_len += sizeof(head);

    memcpy(data + ret_len, command_line, line_size);
    ret_len += line_size;

    *res_len = ret_len;
    return true;
}

bool inline pack_command_data_pack(const char* data, size_t input_len, char* out, size_t out_len, size_t *res_len) {
    size_t ret_len = 0;
    pack_type_t type = DATA_PACK;

    if (ret_len + sizeof(type) + input_len > out_len) {
        return false;
    }

    *(pack_type_t *) data = type;
    ret_len += sizeof(type);

    memcpy(out + ret_len, data, input_len);
    ret_len += input_len;

    *res_len = ret_len;
    return true;
}

bool inline pack_command_stream_info(command_stream_info_req_head *head, char* data, size_t len, size_t *res_len) {
    size_t ret_len = 0;
    pack_type_t  type = COMMAND_STREAM_INFO;

    if (ret_len + sizeof(type) > len) {
    return false;
    }

    *(pack_type_t *) data = type;
    ret_len += sizeof(type);

    proto_stream_info_req_hton(head);
    if (ret_len + sizeof(*head) > len) {
        return false;
    }

    *(command_stream_info_req_head *) (data + ret_len) = *head;
    ret_len += sizeof(*head);

    *res_len = ret_len;
    return true;
}

#endif //FLUENT_BIT_STORAGE_COMMAND_H
