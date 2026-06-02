# Proc#<< and Proc#>> composition operators
double = ->(x) { x * 2 }
increment = ->(x) { x + 1 }

# >> pipes left to right: increment then double
inc_then_double = increment >> double
p inc_then_double.call(3)   # (3+1)*2 = 8

# << pipes right to left: double then increment
double_then_inc = increment << double
p double_then_inc.call(3)   # (3*2)+1 = 7

# Chaining three functions
square = ->(x) { x * x }
pipeline = increment >> double >> square
p pipeline.call(2)   # ((2+1)*2)^2 = 36

# Works with Method objects
upcase_m = method(:puts)
to_s_then_upcase = :upcase.to_proc << :to_s.to_proc
p "hello".then(&to_s_then_upcase)

# compose returns a Proc
p (double >> increment).class
p (double << increment).class

# Works with proc {}
add_ten = proc { |x| x + 10 }
mul_three = proc { |x| x * 3 }
p (add_ten >> mul_three).call(5)   # (5+10)*3 = 45
p (add_ten << mul_three).call(5)   # (5*3)+10 = 25
