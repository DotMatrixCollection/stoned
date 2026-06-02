Point = Struct.new(:x, :y)
p Point.members
p Point.new(1, 2).to_h
p Point.new(3, 4).values
p Point.new(1, 2) == Point.new(1, 2)
p Point.new(1, 2) == Point.new(1, 3)
