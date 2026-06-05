add = proc { |a, b| a + b }
one = proc { |x| x }
p add.arity
p one.arity
p [1, 2, 3].map { |n| add.call(n, 10) }
