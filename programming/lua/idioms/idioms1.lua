--******************************************************************************
local line = io.read()
if line then  -- instead of line ~= nil
  ...
end
...
if not line then  -- instead of line == nil
  ...
end

--******************************************************************************
local function test(x)
  x = x or "idunno"
    -- rather than if x == false or x == nil then x = "idunno" end
  print(x == "yes" and "YES!" or x)
    -- rather than if x == "yes" then print("YES!") else print(x) end
end

--******************************************************************************



