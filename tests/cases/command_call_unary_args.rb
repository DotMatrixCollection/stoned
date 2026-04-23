def capture(x)
  puts x
end

def pass
  yield -3
  yield +4
end

class Box
  def capture(x)
    puts x
  end
end

class Base
  def capture(x)
    puts x
  end
end

class Child < Base
  def capture(x)
    super -x
    super +x
  end
end

box = Box.new

puts -1
puts +2
capture -3
capture +4
box.capture -5
box.capture +6
pass { |x| puts x }
Child.new.capture(7)
