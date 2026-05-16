Point = Struct.new(:x, :y) do
  def to_s
    "(#{x}, #{y})"
  end
  def distance
    Math.sqrt(x**2 + y**2).round(3)
  end
end
p = Point.new(3, 4)
puts p.to_s
puts p.distance
puts Point.members.inspect
