Point = Struct.new(:x, :y) do
  def distance_to(other)
    Math.sqrt((x - other.x)**2 + (y - other.y)**2)
  end

  def to_s
    "(#{x}, #{y})"
  end
end

p1 = Point.new(0, 0)
p2 = Point.new(3, 4)
p p1.x
p p2.y
p p1.to_s
p p2.to_s
d = p1.distance_to(p2)
p d.round(4)

p Point.members
p p1 == Point.new(0, 0)
p p1 == p2
