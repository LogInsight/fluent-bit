--
-- Created by IntelliJ IDEA.
-- User: ma
-- Date: 15/12/19
-- Time: 下午1:35
-- To change this template use File | Settings | File Templates.
--


require("config")

local inotify = require("linotify")

inotify.init(10, 8, "127.0.0.1", 12800)

local files = get_file()

for k, v in pairs(files) do
    for k1, v1 in pairs(v.file)do
        print(k1, v1)
    end
end
print(files)
for _, config in pairs(files) do
    print("config.path", config.path)
    local wfd =  inotify.add_watch(config.path)
    for _, file in pairs(config.file) do
        print(file, wfd)
        inotify.open(wfd, file, 0)
        local lines = inotify.read_lines(wfd, file)
        for _, line in pairs(lines) do
            print(line)
        end
    end
end

while 1 do
    print("while")
    local events = inotify.read_events()
    if events then
        for _, event in pairs(events) do
            print ("event",event.wd, event.mask, event.cookie, event.name)
        end
    end

end


