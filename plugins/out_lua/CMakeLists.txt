
set(src
  ../../src/flb_network.c
  storage_lua.c
  storage_command.c
  storage_lua_meta.c
  storage_pack.c)

FLB_PLUGIN(out_lua "${src}" "")
