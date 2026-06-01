Pair = Struct.new(:a, :b)
p = Pair.new(1, 2)
out = []
p.each_pair { |k, v| out << [k, v] }
p out
p p.each.to_a
