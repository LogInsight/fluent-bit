--
-- Created by IntelliJ IDEA.
-- User: ma
-- Date: 15/12/17
-- Time: 下午3:45
-- To change this template use File | Settings | File Templates.
--

require ("config")
local inotify = require("inotify")

local exports = {}

exports.watch_fd = {}
exports.watch_path = {}

local event_table = {}

local movein_table = {}
local moveout_table = {}
local move_flags = false
function exports.data_decode(lines)
    if not lines then
        return {}
    end

    return lines
end

function exports.data_send(lines)
    --local event_num = 0
    local send_data = ''
    if lines then
        for _, line in pairs(lines) do
--            inotify.send(line, #line)
            --event_num = event_num + 1
            send_data = send_data .. line
--[[
            event_num = event_num + 1
            if event_num >= 10000 then
                print(#send_data)
                inotify.send(send_data, #send_data)
                event_num = 0
                send_data = ''
            end
--]]
        end
--        print("end", #send_data)
        inotify.send(send_data, #send_data)
    end
end

function exports.init (ip, port, host, timeout, file_size)
    if not timeout then
        timeout = 1
    end

    if not file_size then
        file_size = 1
    end
    inotify.init(timeout, file_size, ip, port, host)
end

function exports.add_watch(path)
    --print(path, exports.watch_path[path]);
    if path and not (path == '') and (not exports.watch_path[path]) then
        local wfd = inotify.add_watch(path)
        --print("add watch", wfd)
        if wfd then
            exports.watch_fd[wfd] = {}
            exports.watch_fd[wfd].file_fd = {}
            exports.watch_fd[wfd].file = {}
            exports.watch_fd[wfd].path = path
            exports.watch_path[path] = wfd
        end
        return wfd
    end
end

function exports.rm_watch(wfd)
    if wfd and exports.watch_fd[wfd] then
        for _, fd in pairs(exports.watch_fd[wfd].file_fd) do
            inotify.close(fd)
        end

        inotify.rm_watch(wfd)
        exports.watch_path[exports.watch_fd[wfd]] = nil
        exports.watch_fd[wfd] = nil
    end
end

function exports.open(wfd, file, offset)
    if not offset then
        offset = 0
    end
    --print("open",wfd, file, exports.watch_fd[wfd])
    if wfd and file and exports.watch_fd[wfd] then
        file_path = exports.watch_fd[wfd].path .. '/' ..file
        local fd = inotify.open(file_path, offset)
        --print(fd)
        if fd then
            --print("open success.\r\n")
            exports.watch_fd[wfd].file_fd[fd] = file
            exports.watch_fd[wfd].file[file] = fd
        end
    end
end

function exports.close(wfd, file)
    if wfd and file and exports.watch_fd[wfd] then
        local fd = exports.watch_fd[wfd].file[file]
        if fd then
            inotify.close(fd)
        end
    end
end

function exports.read_events()
    local events = inotify.read_event()
    if events then
        for _, event in pairs(events) do
            --print(event.wd, event.mask, event.cookie, event.name)
            if exports.event_table[event.mask] then
                exports.event_table[event.mask](event.wd, event.name, event.cookie)
            end
        end
    else
        print("timeout");
    end

end

function exports.read_lines(wfd, file)
    if wfd and file then
        if exports.watch_fd[wfd]  then
            local fd = exports.watch_fd[wfd].file[file]
            if fd then
                return inotify.read_lines(fd)
            end
        end
    end
end

local function file_modify(wfd, name, cookie)
    local lines = exports.read_lines(wfd, name)
    lines = exports.data_decode(lines)
    exports.data_send(lines)
    --print('modify', wfd, name, cookie)
end

local function file_move_in(wfd, name, cookie)
    move_flags = true
    if not movein_table[wfd] then
        movein_table[wfd] = {}
    end
    movein_table[wfd][cookie] = name
    --print('move in', wfd, name, cookie)

    if moveout_table[wfd] and moveout_table[wfd][cookie] then
        print( moveout_table[wfd][cookie].." change to ".. name)
    end
    exports.event_table.event_end()
end

local function file_move_out(wfd, name, cookie)
    exports.close(wfd, name)
    move_flags = true
    if not moveout_table[wfd] then
        moveout_table[wfd] = {}
    end
    moveout_table[wfd][cookie] = name
    --print('move out', wfd, name, cookie)
    if movein_table[wfd] and movein_table[wfd][cookie] then
         print (name .. " change to " .. movein_table[wfd][cookie])
    end

end

local function file_event_end()
    if move_flags then
        --print("move end")
        movein_table = {}
        moveout_table = {}
        move_flags = false
    end

end

local function file_create(wfd, name, cookie)
    --print('create', wfd, name, cookie)
    return exports.open(wfd, name, 0)
end


local function file_delete(wfd, name, cookie)
    --print('delete', wfd, name, cookie)
    exports.close(wfd, name)
end

event_table[inotify.IN_CREATE] = file_create
event_table[inotify.IN_DELETE] = file_delete
event_table[inotify.IN_MODIFY] = file_modify
event_table[inotify.IN_MOVED_TO] = file_move_in
event_table[inotify.IN_MOVED_FROM] = file_move_out
event_table.event_end = file_event_end

exports.event_table = event_table

return exports

