--
-- Created by IntelliJ IDEA.
-- User: ma
-- Date: 15/12/19
-- Time: 下午1:35
-- To change this template use File | Settings | File Templates.
--


require("config")

local inotify = require("linotify")

inotify.init("192.168.200.206", 3303, "192.168.200.238", 2, 2)

local files = get_file()

for _, config in pairs(files) do
    --print("config.path", config.path)
    local wfd =  inotify.add_watch(config.path)
    for _, file in pairs(config.file) do
        --print(file, wfd)
        inotify.open(wfd, file, 0)
        local time1 = os.clock()
        local lines = inotify.read_lines(wfd, file)
        while lines do
            --lines = inotify.data_decode(lines)
            inotify.data_send(lines)
            lines = inotify.read_lines(wfd, file)
        end
        print(os.clock() - time1)
    end
end

print('start wait event')

while 1 do
    inotify.read_events()
end

