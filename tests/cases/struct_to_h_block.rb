Point = Struct.new(:x, :y)
pt = Point.new(3, 4)
p pt.to_h
p pt.to_h { |k, v| [k.to_s, v * 2] }
p pt.to_h { |k, v| [k, v.to_f] }

Named = Struct.new(:first, :last)
n = Named.new("Alice", "Smith")
p n.to_h { |k, v| [v, k] }
