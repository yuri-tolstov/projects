#!/usr/bin/lua
--******************************************************************************
--==============================================================================
-- Example 1
--==============================================================================
--[[
mystring = "C[1:2:3] 5 7"

for s in string.gmatch(mystring, "([^".."%s".."]+)") do
   print(s)
end
--]]

--==============================================================================
-- Example 2
--==============================================================================
--[[
mystring = "C[1:2:3] 5 7"
t = {}; i = 1

for s in string.gmatch(mystring, "([^".."%s".."]+)") do
   t[i] = s
   i = i + 1
end

-- FOR begin, end, step DO ... END
for si = 1, i, 1 do
   print(t[si])
end
--]]

--==============================================================================
-- Example 3
-- Extract numbers between [ and ] separated by :
--==============================================================================
mystring = "C[1:2:3] 5 7"
t = {}; i = 1
for s in string.gmatch(mystring, "([^".."%s".."]+)") do
   t[i] = s
   i = i + 1
end
tt = {}; i = 1
for s in string.gmatch(t[1], "([^".."[".."]+)") do
   tt[i] = s
   i = i + 1
end
ttt = {}; i = 1
for s in string.gmatch(tt[2], "([^".."]".."]+)") do
   ttt[i] = s
   i = i + 1
end
tttt = {}; i = 1
for s in string.gmatch(ttt[1], "([^"..":".."]+)") do
   tttt[i] = s
   i = i + 1
end
for si = 1, i - 1, 1 do
   print(tttt[si])
end

