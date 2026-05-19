# Float#to_r
puts 0.5.to_r.class     # Rational
puts 0.5.to_r.inspect   # (1/2)
puts 0.75.to_r.inspect  # (3/4)
puts 0.25.to_r.inspect  # (1/4)
puts 0.1.to_r.class     # Rational (approximate)

# Integer#to_r
puts 5.to_r.inspect     # (5/1)
puts 0.to_r.inspect     # (0/1)
puts (-3).to_r.inspect  # (-3/1)

# String#to_r
puts "3/4".to_r.inspect    # (3/4)
puts "7".to_r.inspect      # (7/1)
puts "-2/3".to_r.inspect   # (-2/3)

# String#to_c
puts "3+2i".to_c.inspect   # (3+2i)
puts "3".to_c.inspect      # (3+0i)
puts "2i".to_c.inspect     # (0+2i)

# Rational#frozen?
puts Rational(1, 2).frozen?  # true
puts Complex(1, 2).frozen?   # true
