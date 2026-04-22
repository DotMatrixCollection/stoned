def outer(x, y = nil)
  puts [x, y].inspect
end

def inner(x)
  x + 1
end

class Box
  def inner(x)
    x + 1
  end
end

outer inner 1, 2
outer(inner(1), 2)

box = Box.new
outer box.inner 1, 2
outer(box.inner(1), 2)
