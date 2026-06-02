Point = Struct.new(:x, :y)

point = Point.new(1, 2)
p point.to_h

point[:x] = 10
point.y = 20
p point.to_a

p point.each_pair.map { |name, value| [name, value * 2] }
