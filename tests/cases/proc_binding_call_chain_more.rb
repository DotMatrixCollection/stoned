x = 10
adder = proc { |n| x + n }
p adder.call(5)
x = 20
p adder.call(5)
