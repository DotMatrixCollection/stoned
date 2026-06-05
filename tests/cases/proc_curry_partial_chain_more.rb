adder = proc { |a, b, c| a + b + c }
add_one = adder.curry.call(1)
p add_one.call(2).call(3)
p adder.curry.call(4).call(5).call(6)
