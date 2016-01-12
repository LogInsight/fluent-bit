//
// Created by jc on 5/1/2016.
//

//#include "storage_type.h"
#include "string.h"
#include "stdbool.h"
#include "storage_lua.h"

#ifndef FLUENT_BIT_STORAGE_COMMAND_H
#define FLUENT_BIT_STORAGE_COMMAND_H

bool storage_process_connect(struct flb_out_lua_config *ctx);

bool storage_process_connect_close(struct flb_out_lua_config *ctx);

size_t parse_data_type(struct flb_out_lua_config *ctx, void *data, size_t len);

#endif //FLUENT_BIT_STORAGE_COMMAND_H
