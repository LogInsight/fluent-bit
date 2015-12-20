--
-- Created by IntelliJ IDEA.
-- User: ma
-- Date: 15/12/17
-- Time: 下午3:39
-- To change this template use File | Settings | File Templates.
--

local lfs = require("lfs")

local function get_real_file(file_table)
    return 'a.txt'
end


CONFIG = {
    {
        path = '.',
        file = {{file = '%S%.txt', func = get_real_file,},}
    },
}

function GetFile(path)
    local files = {}
    local file_num = 1
    for entry in lfs.dir(path) do
        if entry ~= '.' and entry ~= '..' then
            local current_path = path .. '/' .. entry
            local attr = lfs.attributes(current_path)
            if attr then
                if attr.mode == "directory" then
                    --GetFile(current_path)
                else
                    files[file_num] = entry
                    file_num = file_num + 1
                    print(entry);
                end
            end
        end
    end
    return files
end

function extra_file(files, extra)
    local file_res = {}
    for _, file in pairs(files) do
        print(file)
        for key_num, rex in pairs(extra) do
            local data = file:match(rex.file)
            print("data", data)
            if data then
                if not file_res[key_num] then
                    file_res[key_num] = {}
                    file_res[key_num].file = {}
                    file_res[key_num].num = 0
                    file_res[key_num].func = rex.func
                end
                file_res[key_num].num = file_res[key_num].num + 1
                file_res[key_num].file[file_res[key_num]] = data
                break
            end
        end
    end
    return file_res
end

function get_file()
    local res = {}
    local path_num = 1
    local file_num = 1
    for _, config in pairs(CONFIG) do
        res[path_num] = {}
        res[path_num].path = config.path
        res[path_num].file = {}
        file_num = 1
        local files = GetFile(config.path)
        local file_res = extra_file(files, config.file)
        for _, file in pairs(file_res) do
            if file.func then
                res[path_num].file[file_num] = file.func(file.file)
                print(res[path_num].file[file_num])
                file_num = file_num + 1
            end
        end
    end
    return res
end
