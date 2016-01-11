//
// Created by mayabin on 16/1/5.
//

#ifndef FLUENT_BIT_IN_LUA_EXEC_H
#define FLUENT_BIT_IN_LUA_EXEC_H

#include <fluent-bit/flb_lua_common.h>
#include "in_lua.h"

void in_lua_exec_init(struct flb_in_lua_config *ctx);
void in_lua_exec_read(struct flb_in_lua_config *ctx, struct flb_in_lua_exec_info *exec);

#endif //FLUENT_BIT_IN_LUA_EXEC_H
