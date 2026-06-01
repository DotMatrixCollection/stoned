S = Struct.new(:a, :b)
s = S.new(1, 2)
p s.deconstruct
p s.deconstruct_keys([:a])
p s.deconstruct_keys(nil)
