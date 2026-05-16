um = Object.instance_method(:inspect)
puts um.bind_call(7)

class Box
  def ping(x)
    x + 1
  end
end

puts Box.instance_method(:ping).bind_call(Box.new, 4)
