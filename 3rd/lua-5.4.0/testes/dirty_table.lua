function string.split( line, sep, maxsplit ) 
	if string.len(line or "") == 0 then
		return {}
	end
	sep = sep or ' '
	maxsplit = maxsplit or 0
	local retval = {}
	local pos = 1   
	local step = 0
	while true do   
		local from, to = string.find(line, sep, pos, true)
		step = step + 1
		if (maxsplit ~= 0 and step > maxsplit) or from == nil then
			local item = string.sub(line, pos)
			table.insert( retval, item )
			break
		else
			local item = string.sub(line, pos, from-1)
			table.insert( retval, item )
			pos = to + 1
		end
	end     
	return retval  
end

function get_dirty_keymap(keys)
    local keymap = {}
    local key_list = string.split(keys, ",")
    for _, key in pairs(key_list) do
        keymap[key] = true
    end

    return keymap
end

function test_manage()
    local dirty_t = {}
    table.begin_dirty_manage(dirty_t, "root")
    table.dump_dirty_root_manage(dirty_t)
    table.clear_dirty_manage(dirty_t)
end

function test_op()
    local dirty_t = {}
    table.begin_dirty_manage(dirty_t, "root")
    dirty_t.c = 1

    dirty_t.a = {}
    dirty_t.a.a1 = 1
    dirty_t.a.a2 = 2

    dirty_t.b = {}
    local t_b = dirty_t.b
    t_b.b1 = 1
    t_b.b2 = 2

    local Flag, keys = table.dump_dirty_root_manage(dirty_t)
    assert(Flag and keys)
    local keymap = get_dirty_keymap(keys)

    print("DIRTY_KEYS: ", keys)

    assert(keymap["s@root.s@c"])

    assert(keymap["s@root.s@a"])
    assert(not keymap["s@root.s@a.s@a1"])
    assert(not keymap["s@root.s@a.s@a2"])

    assert(keymap["s@root.s@b"])
    assert(not keymap["s@root.s@b.s@b1"])
    assert(not keymap["s@root.s@b.s@b2"])

    table.clear_dirty_manage(dirty_t)
    local Flag, keys = table.dump_dirty_root_manage(dirty_t)
    assert(Flag and not keys)

    dirty_t.c = "2"

    dirty_t.a.a1 = "11"
    dirty_t.a.a2 = "22"

    dirty_t.b.b1 = 11
    dirty_t.b.b2 = "22"
    dirty_t.b.b3 = "23"

    local Flag, keys = table.dump_dirty_root_manage(dirty_t)
    assert(Flag and keys)
    local keymap = get_dirty_keymap(keys)
    print("DIRTY_KEYS: ", keys)

    assert(not keymap["s@root.s@a"])
    assert(not keymap["s@root.s@b"])
    assert(keymap["s@root.s@c"])

    assert(keymap["s@root.s@a.s@a1"])
    assert(keymap["s@root.s@a.s@a2"])

    assert(keymap["s@root.s@b.s@b1"])
    assert(keymap["s@root.s@b.s@b2"])
    assert(keymap["s@root.s@b.s@b3"])

    table.clear_dirty_manage(dirty_t)
    local Flag, keys = table.dump_dirty_root_manage(dirty_t)
    assert(Flag and not keys)
end

function test_array()
    local dirty_t = {}
    table.begin_dirty_manage(dirty_t, "root")
    for i = 1, 10 do
        dirty_t[i] = 1
    end
    local Flag, keys = table.dump_dirty_root_manage(dirty_t)
    print(Flag, keys)

    for i = 1, 10 do
        dirty_t[i] = i * 10
    end

    local Flag, keys = table.dump_dirty_root_manage(dirty_t)
    print(Flag, keys)

    for i = 1, 10, 2 do
        dirty_t[i] = nil
    end

    local Flag, keys = table.dump_dirty_root_manage(dirty_t)
    print(Flag, keys)

    dirty_t = nil
    collectgarbage("collect")
end

function test_free()
    local dirty_t = {}
    table.begin_dirty_manage(dirty_t, "root")
    dirty_t = nil
    collectgarbage("collect")

    local dirty_t = {}
    table.begin_dirty_manage(dirty_t, "root")

    dirty_t.c = 2

    dirty_t.a = {}
    dirty_t.a.a1 = 11
    dirty_t.a.a2 = 22
    table.clear_dirty_manage(dirty_t)

    dirty_t.c = "c"
    dirty_t.a.a1 = "a1"
    dirty_t.a.a2 = "a2"

    dirty_t = nil

    collectgarbage("collect")
end

function test_api()
    local dirty_t = {}
    table.begin_dirty_manage(dirty_t, "root")
    for i = 1, 5 do
        table.insert(dirty_t, i)
    end
    local Flag, keys = table.dump_dirty_root_manage(dirty_t)
    print("insert tail: ", keys)
    table.clear_dirty_manage(dirty_t)

    for i = 1, 5 do
        table.remove(dirty_t)
    end

    local Flag, keys = table.dump_dirty_root_manage(dirty_t)
    print("remove tail: ", Flag, keys)
    table.clear_dirty_manage(dirty_t)

    for i = 1, 5 do
        table.insert(dirty_t, 1, i)
    end

    local Flag, keys = table.dump_dirty_root_manage(dirty_t)
    print("insert head: ", Flag, keys)
    table.clear_dirty_manage(dirty_t)

    for i = 1, 5 do
        table.remove(dirty_t, 1)
    end

    local Flag, keys = table.dump_dirty_root_manage(dirty_t)
    print("remove head: ", Flag, keys)
    table.clear_dirty_manage(dirty_t)

    for i = 1, 5 do
        table.insert(dirty_t, i)
    end

    table.clear_dirty_manage(dirty_t)
    dirty_t[2] = nil
    dirty_t[4] = nil
    local Flag, keys = table.dump_dirty_root_manage(dirty_t)
    print("remove middle: ", Flag, keys)
end


print("----begin dirty data test------")

test_manage()
collectgarbage("collect")

test_op()
collectgarbage("collect")

test_free()
collectgarbage("collect")

test_array()

test_api()
print("----dirty data test end------")
