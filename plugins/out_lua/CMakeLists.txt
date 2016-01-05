
set(src
  ../../src/flb_network.c
  storage_lua.c)

FLB_PLUGIN(out_lua "${src}" "")
