
function test_dirty_performance(key_size)
    local Now = os.clock()
    for i = 1, 100000 do
        local dirty_t = {}
        table.begin_dirty_manage(dirty_t, "root")
        for j = 1, key_size do
            local key = "key" .. j
            dirty_t[key] = key
        end
    end
    return (os.clock() - Now)
end

function test_normal_performance(key_size)
    local Now = os.clock()
    for i = 1, 100000 do
        local dirty_t = {}
        for j = 1, key_size do
            local key = "key" .. j
            dirty_t[key] = key
        end
    end

    return (os.clock() - Now)
end

local KEY_SIZE = 100
local KEY_STEP = 5
local KEY_START = 10

local dirty_cost = {}
local normal_cost = {}
for i = KEY_START, KEY_SIZE, KEY_STEP do
    table.insert(dirty_cost, test_dirty_performance(i))
end

for i = KEY_START, KEY_SIZE, KEY_STEP do
    table.insert(normal_cost, test_normal_performance(i))
end


f = io.open(arg[1], "w+")
local col = KEY_START
for i = 1, #dirty_cost do
    local row = {col, dirty_cost[i], normal_cost[i], "\n"}
    f:write(table.concat(row, ","))
    col = col + KEY_STEP
end

f:close()

print(table.concat(dirty_cost, ","))
print(table.concat(normal_cost, ","))