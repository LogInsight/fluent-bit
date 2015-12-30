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

Embed LUA in C
===========================
需要的动作，

- 确定最新的文件
- 安排文件上传？   -> python script
- 处理过滤最新的文件
- 安排 多行事件的起始、结束 Token   
- 对数据进行过滤       -> 使用 msgpack 编码？       单行事件, 多行事件... 
- 注册文件是否监控？
- 
1 create a input reactor in  LUA, pass all configure keys
    - create_file_input
    - get_file_input
      
2 reg file to mon
3 notify to lua if inotify


 
 
设定配置文件格式
=======================
 1 因为 配置文件为 Python 生成的, 需要 LUA 和 Python 都便于支持的配置文件格式
 
在 flb 中， 使用 monkey 库 提供的 配置文件解析模块进行配置文件读取， 配置文件类似 INI；

对应到 LUA Input 中， 采用如下的形式

[FILE]             # FileInput 实际就是 LUA Input， 因为默认就是 LUA 的， 不额外体现 LUA
  # 下面， 每个 带 input 前缀的 key 是 一个 有效的数据, 形如
  file_nginx_access = True
  在 节
  [File:nginx_access]
  中， 含有对具体输入条目的配置
  ;
  当 对应的值为 False 或 0， 则 该数据输入不启用
  
  在 具体的条目下， 有如下输入项
  - journal_directory 记录当前文件数据传输位置的文件，为一个 JSON （或纯文本？）
  - log_directory 要监控的日志文件
  - file_match  监控文件名的 match， 因为日志目录可能有多种文件
  - rescan_interval 重新的扫描、读取间隔
  - priority 符合 file_match 规则的文件的前后排序规则，只有最前面的才会被实时监控

[EXEC]              # 实际就是定期执行一个 命令，从其 stdout 或/和 stderr 中读取数据记录（作为日志）
  每个带 exec_ 的 key 是一个 需要定期执行的 命令， 命令可以是 shell ，也可以是 lua 脚本
  [Exec:up_time]
    - refresh_interval  重新执行的时间
    - watch = [stdout | stderr | both]      # 当为 both 时，一次执行，出现两条日志
    - shell 如果出现此条，表示是需要定时执行一个 shell 命令
    - call  调用一段系统(用户)预制的 lua 脚本

[STAT]
    类似 exec ，但是需要保证每次执行都返回一个或一组特定格式的数据
    - hostname
    - refresh_interval  重新执行的时间
    - format 文本数据的格式
    
！！！ 此配置文件的格式要求比较严格，需要严格缩进    
  

[LS]
  存在一系列 lua_ 的扩展
  lua_debug = [ON | OFF] 使用允许使用LUA 的 debug 扩展
  lua_package = [ON | OFF] 是否允许使用 LUA 的 package 扩展
  lua_paths

  - hostname  当前主机名称， 在全局配置文件中也有， 这个主机名称由用户注册当前主机时指定，也可自动计算
    

系统启动的流程

1 [C] 从配置文件中读取要监控那些日志输入
    - 具体的文件
    或
    - 候选的文件列表
  
  init_file(global, stream_key, config) 
  init_exec(global, stream_key, config)
  init_stat(global, stream_key, config)
  
2 [LUA] 对于具体的文件， 直接加入 监控队列
    watch_file
3 [LUA] 对于候选的文件列表， 根据某些具体规则加入监控队列 或 安排上传
    watch_file
    schedule_upload
4 前述的流程 传入当前输入的全部配置信息，作为 hash_table
5 [LUA] 对于需要监控的目录, 需要注册一个回调函数
    watch_path(path_name, call_back)

<需要定时执行的情况>    
2 [LUA] LUA 端不实际执行，至少定时收到 std_out 和 std_err 进行数据解析，
  解析完毕后， 调用
     send_event(stream_key, the_event)
  - 在 插件一层，可以 通过 stream_key 对 输入进行追踪
    不明面上调用 获取当前时间，由 API 内部读取当前时间
    
<对于 stat>    
2 [LUA] 定时接受输入，进行解析
    解析完毕后，调用
    send_metric_string(stream_key, metric_key, value)
    send_metric_integer(stream_key, metric_key, value)
    send_metric_double(stream_key, metric_key, value)
    不明面上调用 获取当前时间，由 API 内部读取当前时间
   
<当收到数据时>
  
2  [LUA] 对一批数据进行解析，然后重新编码
  
    process(global, input_msg_enc_data)
    
    send_to_server(global, output_msg_enc_data）
    
    编码的信息应该包括 stremd_id，raw_data
    
    lua 脚本 调用 各个 input 的实际数据处理函数( 根据 stream_id 的映射关系表 ）

  
 
 
