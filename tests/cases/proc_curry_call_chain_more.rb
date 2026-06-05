mul = proc { |a, b| a * b }
twice = mul.curry.call(2)
p twice.call(7)
p [1, 2, 3].map(&twice)
