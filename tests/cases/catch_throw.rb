result = catch(:done) do
  throw :done, 42
  "never"
end
puts result

result2 = catch(:x) do
  throw :x
end
puts result2.inspect
