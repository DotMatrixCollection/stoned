$stdout.sync = true

# Proc#>> and Proc#<< composition operators

double = ->(x) { x * 2 }
inc    = ->(x) { x + 1 }
square = ->(x) { x * x }

# >> pipes left-to-right: f >> g means g(f(x))
puts (double >> inc).(5)     # 11  (double then inc: 5*2=10, 10+1=11)
puts (inc >> double).(5)     # 12  (inc then double: 5+1=6, 6*2=12)
puts (double >> inc >> square).(3)  # 49  (3*2=6, 6+1=7, 7*7=49)

# << pipes right-to-left: f << g means f(g(x))
puts (double << inc).(5)     # 12  (inc first: 5+1=6, then double: 6*2=12)
puts (inc << double).(5)     # 11  (double first: 5*2=10, then inc: 10+1=11)

# Works with procs too
add5 = proc { |x| x + 5 }
puts (double >> add5).(3)    # 11 (3*2=6, 6+5=11)

# Composed proc is a lambda
puts (double >> inc).lambda?   # true
