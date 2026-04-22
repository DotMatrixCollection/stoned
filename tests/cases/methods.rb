def greet(name = "world")
  "hi #{name}"
end

def collect(head, *rest)
  puts head
  puts rest.length
end

puts greet
puts greet("ruby")
collect(10, 20, 30, 40)
