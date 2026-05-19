# Integer#methods includes primitive operators and type methods
m = 42.methods
puts m.include?(:+)       # true
puts m.include?(:times)   # true
puts m.include?(:gcd)     # true
puts m.include?(:to_f)    # true
puts m.include?(:even?)   # true

# String#methods includes string-specific methods
sm = "hi".methods
puts sm.include?(:upcase)   # true
puts sm.include?(:gsub)     # true
puts sm.include?(:split)    # true

# Array#methods
puts [].methods.include?(:push)    # true
puts [].methods.include?(:select)  # true

# Hash#methods
puts({}.methods.include?(:merge))  # true
puts({}.methods.include?(:keys))   # true

# Object builtins present on all types
puts 42.methods.include?(:freeze)       # true
puts 42.methods.include?(:respond_to?)  # true
puts "x".methods.include?(:class)       # true
