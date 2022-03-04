table opcode desc
==================
OP_SETI
```lua
local t = {}
t[1] = 1
```
-------------
OP_SETFIELD
```lua
local t = {}
t.a = "aaa"
```
-------------
OP_SETLIST
```lua
local t = {1,2,3}
```

-------------
OP_SETTABLE
```lua
local t1 = {
    a = "aaa"
}
t1[t1.a] = "bbb"
```

Table api operation
===========
table.insert
```lua
local t = {}
table.insert(t, 1)
```

------------
table.remove
```lua
local t = {1, 2, 3}
table.remove(t)
```