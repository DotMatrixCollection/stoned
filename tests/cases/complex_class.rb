# Complex kernel function and class
puts Complex(3, 4).inspect    # (3+4i)
puts Complex(3, -4).inspect   # (3-4i)
puts Complex(5).inspect       # (5+0i)
puts Complex(3, 4).class      # Complex

# Accessors
c = Complex(3, 4)
puts c.real        # 3
puts c.imaginary   # 4
puts c.imag        # 4

# Arithmetic
puts (Complex(1, 2) + Complex(3, 4)).inspect   # (4+6i)
puts (Complex(5, 3) - Complex(2, 1)).inspect   # (3+2i)
puts (Complex(2, 3) * Complex(1, 2)).inspect   # (-4+7i)

# Abs
puts Complex(3, 4).abs    # 5.0
puts Complex(3, 4).abs2   # 25

# Conjugate
puts Complex(3, 4).conjugate.inspect  # (3-4i)
puts Complex(3, 4).conj.inspect       # (3-4i)

# Equality
puts Complex(1, 2) == Complex(1, 2)  # true
puts Complex(1, 2) == Complex(1, 3)  # false
puts Complex(5, 0) == 5              # true

# Predicates
puts Complex(3, 4).real?    # false
puts Complex(0, 0).zero?    # true
puts Complex(1, 0).zero?    # false

# Rectangular
puts Complex(3, 4).rectangular.inspect  # [3, 4]
puts Complex(3, 4).rect.inspect         # [3, 4]
