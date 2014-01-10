--------------------------------------------------------------------------------
-- This implementation is based on C-file found on the Web
--------------------------------------------------------------------------------
function partition(t, first, last)
   local d
   local pi = first
   local i = first
   local k = last

   while (i < k) do
      while (t[i] <= t[pi] and i <= last) do i = i + 1 end
      while (t[k] > t[pi]) do k = k - 1 end
      if (i >= k) then
         break;
      end
      d = t[i];  t[i] = t[k];  t[k] = d
   end
   d = t[pi];  t[pi] = t[k];  t[k] = d
   return k
end

function quicksort(t, first, last)
   if (first < last) then
      local k = partition(t, first, last)
      t = quicksort(t, first, k - 1)
      t = quicksort(t, k + 1, last)
   end
   return t
end

--------------------------------------------------------------------------------
-- Program:
--------------------------------------------------------------------------------
-- print(unpack(quicksort{2, 3, 8, 1, 5, 9, 4}))
local t = {2, 3, 8, 1, 5, 9, 6, 2}
print(unpack(quicksort(t, 1, #t - 1)))

