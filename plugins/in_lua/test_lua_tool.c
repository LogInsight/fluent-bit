#include "in_lua_tool.h"

#define BUF_LEN 1024
char dir[BUF_LEN];
char file_name[BUF_LEN];

int main() {

    Get_Last_File(".", 1, file_name, BUF_LEN);

    printf("qqqqqqqq: %s\n", file_name);
    return 0;
}
