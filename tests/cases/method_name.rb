def greet
  puts __method__
end

def outer
  puts __method__
end

greet
outer
puts __method__.inspect
