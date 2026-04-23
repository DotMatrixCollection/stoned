def wrap_arg(x)
  [x]
end

def yield_wrap
  yield ("ab").upcase
end

class Box
  def wrap(x)
    [x]
  end
end

class Base
  def wrap(x)
    [x]
  end
end

class Child < Base
  def wrap(x)
    super (x).upcase
  end
end

value = "ab"
box = Box.new

puts (value).upcase
puts wrap_arg ("ab").upcase
puts box.wrap ("cd").upcase
puts yield_wrap { |x| [x] }
puts Child.new.wrap("ef")
