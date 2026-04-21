print("BYTECODE_OK")
-- Complex Lua test to exercise functions and recursion
local function factorial(n)
  if n <= 1 then
    return 1
  else
    return n * factorial(n - 1)
  end
end
local f = factorial(5) -- 120
print("factorial(5)=", f)

local function sum(a,b)
  return a + b
end
local s = sum(3,7) -- 10
print("3+7=", s)
return f + s
