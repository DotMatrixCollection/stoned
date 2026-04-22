def outer(x)
  yield x if true
end

def inner(x)
  x + 1
end

class Box
  def inner(x)
    x + 1
  end
end

puts outer inner 3 do |n|
  n + 1
end
puts outer(inner(3)) do |n|
  n + 1
end

box = Box.new
puts outer box.inner 3 do |n|
  n + 1
end
puts outer(box.inner(3)) do |n|
  n + 1
end

def passthrough(x)
  x
end

def with_block(x)
  yield x if true
end

puts passthrough with_block(3) { |n| n + 1 }
puts passthrough(with_block(3) { |n| n + 1 })
