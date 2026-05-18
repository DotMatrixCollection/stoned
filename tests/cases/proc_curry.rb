$stdout.sync = true

# Proc#curry — partial application accumulation

add = ->(a, b) { a + b }

# Single step: curry then apply
c = add.curry
partial = c.(5)
puts partial.(3)      # 8

# Both args at once via curried
puts add.curry.(5, 3) # 8

# Three-argument lambda
mul = ->(a, b, c) { a * b * c }
puts mul.curry.(2).(3).(4)   # 24
puts mul.curry.(2, 3).(4)    # 24
puts mul.curry.(2).(3, 4)    # 24

# Curried proc (not lambda)
times = proc { |a, b| a * b }
puts times.curry.(6).(7)  # 42

# curry returns a Proc
puts add.curry.class       # Proc
