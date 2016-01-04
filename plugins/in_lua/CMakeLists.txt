# FIXME: there is something wrong when linking objects and this
# static plugin, I should not require to link to a specific symbol
# if the object was already linked from fluent-bit core on src/, also
# jsmn should not be required.

set(src
  in_lua.c
  in_lua_config.c
  in_lua_file.c
  ../../src/flb_pack.c)

include_directories(
        ${CMAKE_SOURCE_DIR}/3rd_party/luajit-2.0/src
        ../lib/
        ../lib/jsmn
        ../lib/msgpack-1.2.0/include
        ${extra_headers}
)

FLB_PLUGIN_3(in_lua "${src}" "jsmn")
