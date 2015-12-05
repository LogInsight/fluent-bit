# FIXME: there is something wrong when linking objects and this
# static plugin, I should not require to link to a specific symbol
# if the object was already linked from fluent-bit core on src/, also
# jsmn should not be required.

set(src
  in_lua.c
  ../../src/flb_pack.c)

link_directories(/usr/lib/x86_64-linux-gnu/)
FLB_PLUGIN_3(in_lua "${src}" "jsmn")
