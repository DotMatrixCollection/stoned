x = 0
result = while x < 3
  x += 1
end
puts result.inspect

result2 = until false
  break 42
end
puts result2

result3 = while true
  break "done"
end
puts result3

y = 0
result4 = begin
  y += 1
end while y < 0
puts result4.inspect
