class Box
  def inner(x)
    yield x if block_given?
    x + 1
  end
end

def pair(a, b)
  puts a
  puts b
end

def trio(a, b, c)
  puts a
  puts b
  puts c
end

box = Box.new

pair((box.inner(3) do |n|
  n * 2
end).succ, box.inner(4).succ)

pair((box.inner(3) do |n|
  n * 2
end), -1)

pair(box.inner(3) do |n|
  n * 2
end, box.inner(4) do |n|
  n * 3
end)

trio((box.inner(3) do |n|
  n * 2
end).succ, 9, 10)
