foo = function(str)
   if (bar == nil) then
      print("Lua: bar is nil")
      return nil
   end
   local s, l = bar(str)
   print("Lua:"..s)
   return l
end

