class Box
  def inner(x)
    yield x if block_given?
    x + 1
  end
end

def capture(x)
  puts x
end

box = Box.new

capture({a: box.inner(3) do |n|
  n * 2
end}[:a])

capture(({a: box.inner(3) do |n|
  n * 2
end, b: 9})[:b])

capture({a: (box.inner(3) do |n|
  n * 2
end).succ}[:a])

capture([box.inner(3) do |n|
  n * 2
end, box.inner(4)][1])
