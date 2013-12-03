function tellme()
  io.write("LUA: tellme function.\n")
end

function square(n)
  io.write("LUA: square function. arg=")
  io.write(tostring(n))
  n = n * n
  io.write(", square=")
  io.write(tostring(n))
  print(".")
  return(n)
end

