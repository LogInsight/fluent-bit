//
// Created by jc on 5/1/2016.
//

#include "storage_type.h"
#include "string.h"
#include "stdbool.h"

#ifndef FLUENT_BIT_STORAGE_PACK_H
#define FLUENT_BIT_STORAGE_PACK_H

bool pack_command_connect(struct command_connect_req_head *connect_head, char* data, size_t len, size_t *res_len);
bool pack_command_close(char* data, size_t len, size_t *res_len);

bool pack_command_stream_start(struct command_stream_start_req_head *head,
                               const char *filename, size_t name_len, char* data, size_t len, size_t *res_len);

bool pack_command_stream_end(struct command_stream_end_req_head *head,
                             char* data, size_t len, size_t *res_len);

bool pack_command_stream(struct command_stream_req_head *head,
                         char* data, size_t len, size_t *res_len);

bool pack_command_substream(struct command_substream_req_head *head,
                            char* data, size_t len, size_t *res_len);

bool pack_command_file(const char *command_line, size_t line_size,
                       char* data, size_t len, size_t *res_len);

bool pack_command_data_pack(const char* data, size_t input_len, char* out, size_t out_len, size_t *res_len);

bool pack_command_file_check(struct command_file_check_req_head *head, char* data, size_t len, size_t *res_len);

#endif //FLUENT_BIT_STORAGE_COMMAND_H
