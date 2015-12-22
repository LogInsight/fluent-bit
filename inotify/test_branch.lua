--
-- Created by IntelliJ IDEA.
-- User: ma
-- Date: 15/12/22
-- Time: 下午4:59
-- To change this template use File | Settings | File Templates.
--



fr = io.open("access.log")
fw = io.open("a.txt", "a")

for line in fr:lines() do
    fw:write(line..'\n')
end

io.close(fr)
io.close(fw)



