# Rational literal suffix r
puts 3r.class         # Rational
puts 3r.inspect       # (3/1)
puts 1r + 2r          # (3/1)
puts (3r / 4).inspect # (3/4)
puts (-5r).inspect    # (-5/1)
puts 1.5r.class       # Rational
puts 1.5r.inspect     # (3/2)

# Imaginary literal suffix i
puts 2i.class         # Complex
puts 2i.inspect       # (0+2i)
puts (-3i).inspect    # (0-3i)
puts 1.5i.class       # Complex

# Integer + Complex coercion
puts (1 + 2i).inspect   # (1+2i)
puts (3 + 0i).inspect   # (3+0i)
puts (5 - 2i).inspect   # (5-2i)
puts (2 * 3i).inspect   # (0+6i)

# Integer + Rational coercion
puts (2 + Rational(1, 2)).inspect  # (5/2)
puts (3 * Rational(1, 3)).inspect  # (1/1)
