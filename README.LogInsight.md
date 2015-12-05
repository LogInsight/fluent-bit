TODO
===================

因为 LUA 脚本与 agent 主进程采用 fork + pipe 通信, 需要
 
 1 提供一个 in_pipe/in_lua 插件, 在插件中 fork 一个子进程, 在子进程中, 返回 一个 简单的 json
   要求
   - agent 主进程能够接收到
   - agent 主进程的in_stdout 能够输出
 
 2 在 in_lua 插件中, 让 lua 脚本输出这个简单的 json 
   
   - 需要确认在 LUA Sandbox 中是否可以输出到 stdout
     (考虑到目前的实现是)
 
 3 让 原型系统 https://github.com/arekinath/loglunatic/ 可以将采集到的日志数据输出的标准输出
 
 4 在 in_lua 插件中, 调用 loglunatic
 
设定配置文件格式
 
 1 因为 配置文件为 Python 生成的, 需要 LUA 和 Python 都便于支持的配置文件格式