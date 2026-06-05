l = ->(x) { x * 2 }
p l.lambda?
p proc { |x| x }.lambda?
p [1, 2, 3].map(&l)
