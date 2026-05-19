# Rational kernel function and class
puts Rational(3, 4).inspect       # (3/4)
puts Rational(2, 4).inspect       # (1/2) — reduced
puts Rational(5).inspect          # (5/1) — default den
puts Rational(3, 4).class         # Rational
puts Rational.new(1, 3).inspect   # (1/3)

# numerator / denominator
r = Rational(7, 12)
puts r.numerator    # 7
puts r.denominator  # 12

# Integer#to_r
puts 4.to_r.inspect   # (4/1)
puts 4.to_r.class     # Rational

# Arithmetic
puts (Rational(1, 2) + Rational(1, 3)).inspect    # (5/6)
puts (Rational(3, 4) - Rational(1, 4)).inspect    # (1/2)
puts (Rational(2, 3) * Rational(3, 4)).inspect    # (1/2)
puts (Rational(2, 3) / Rational(4, 3)).inspect    # (1/2)
puts (Rational(2, 3) ** 2).inspect                # (4/9)

# Mixed arithmetic
puts (Rational(1, 2) + 1).inspect    # (3/2)
puts (Rational(1, 2) * 4).inspect    # (2/1)

# Comparison
puts Rational(1, 2) > Rational(1, 3)   # true
puts Rational(1, 4) < Rational(1, 2)   # true
puts Rational(1, 2) == Rational(2, 4)  # true
puts Rational(1, 2) == Rational(1, 3)  # false

# Conversions
puts Rational(3, 4).to_f           # 0.75
puts Rational(7, 2).to_i           # 3
puts Rational(3, 4).to_r.class     # Rational

# Predicates
puts Rational(3, 4).zero?     # false
puts Rational(0, 1).zero?     # true
puts Rational(3, 4).positive? # true
puts Rational(-1, 2).negative? # true
puts Rational(3, 4).integer?  # false
puts Rational(3, 4).finite?   # true

# Abs and unary
puts Rational(-3, 4).abs.inspect  # (3/4)
puts (-Rational(1, 2)).inspect    # (-1/2)

# Rounding
puts Rational(3, 4).ceil     # 1
puts Rational(3, 4).floor    # 0
puts Rational(3, 4).round    # 1
puts Rational(3, 4).truncate # 0

# Comparable mixin
puts Rational(1, 4).between?(Rational(0, 1), Rational(1, 2))  # true
puts [Rational(3, 4), Rational(1, 4), Rational(1, 2)].sort.map(&:inspect).inspect
# ["(1/4)", "(1/2)", "(3/4)"]
