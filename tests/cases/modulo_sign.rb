$stdout.sync = true

# Ruby modulo: result always has same sign as the divisor

puts  7 % 3     # 1  (positive % positive → positive)
puts -7 % 3     # 2  (negative % positive → positive, Ruby semantics)
puts  7 % -3    # -2 (positive % negative → negative)
puts -7 % -3    # -1 (negative % negative → negative)

# Float modulo
puts  7.0 % 3   # 1.0
puts -7.0 % 3   # 2.0
puts  7.0 % -3  # -2.0
puts -7.0 % -3  # -1.0

# Used in common patterns
puts (0..10).map { |n| n % 3 }.inspect  # [0,1,2,0,1,2,0,1,2,0,1]
puts [1, -1, -4, 7, -7].map { |n| n % 3 }.inspect  # [1,2,2,1,2]
