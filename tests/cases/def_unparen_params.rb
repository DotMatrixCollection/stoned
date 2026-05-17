def add x, y
  x + y
end

def greet name, greeting = "hello"
  "#{greeting}, #{name}"
end

def splat first, *rest
  [first, rest]
end

puts add(3, 4)
puts greet("world")
puts greet("world", "hi")
r = splat(1, 2, 3)
puts r[0]
puts r[1].inspect
