pr = proc { |a, b = 1, *rest| }
p pr.arity
p pr.parameters
lm = ->(a, b = 1, *rest, &blk) { }
p lm.arity
p lm.parameters
