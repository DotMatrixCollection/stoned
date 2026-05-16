Point = Struct.new(:x, :y)
p = Point.new(3, 4)
puts p.x
puts p.y
puts Point.superclass == Struct
