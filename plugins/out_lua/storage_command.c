//
// Created by jc on 5/1/2016.
//

#include "storage_command.h"
#include "storage_type.h"

#include <fluent-bit/flb_utils.h>
#include <fluent-bit/flb_io.h>

void
process_data_body(uint8_t type, const char *data, size_t len) {
    switch (type) {
        case DATA_PACK: {
            break;
        }
        case COMMAND_CONNECT: {
            break;
        }
        case COMMAND_CLOSE: {
            break;
        }
        case COMMAND_STREAM_START: {
            break;
        }
        case COMMAND_STREAM_END: {
            break;
        }
        case COMMAND_STREAM: {
            break;
        }
        case COMMAND_SUBSTREAM: {
            break;
        }
        case COMMAND_FILE: {
            break;
        }
        case COMMAND_STREAM_INFO: {
            break;
        }
        case COMMAND_KEEP_ALIVE: {
            break;
        }
        default: {
            flb_error("not support this command type");
            break;
        }
    }
}