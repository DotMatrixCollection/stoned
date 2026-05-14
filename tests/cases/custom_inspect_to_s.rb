class Point
  def initialize(x, y)
    @x = x
    @y = y
  end

  def to_s
    "(#{@x}, #{@y})"
  end

  def inspect
    "Point(#{@x}, #{@y})"
  end
end

p = Point.new(1, 2)
q = Point.new(3, 4)

p p
puts p
p [p, q]
p({origin: Point.new(0, 0), dest: q})
