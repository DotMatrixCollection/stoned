class Box
  def inner(x)
    yield x if block_given?
    x + 1
  end
end

def id(x)
  x
end

def outer(x)
  x + 10
end

box = Box.new

puts id((box.inner(3) do |n|
  n * 2
end).succ)

puts outer((box.inner(3) do |n|
  n * 2
end).succ)
