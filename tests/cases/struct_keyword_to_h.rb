Point = Struct.new(:x, :y, keyword_init: true)
p = Point.new(x: 1, y: 2)
p p.to_h
p p.values
p p.members
