x = nil
result = x and puts("should not print")
puts result.inspect
y = true
y and puts("yes")
z = false
z or puts("or side")
