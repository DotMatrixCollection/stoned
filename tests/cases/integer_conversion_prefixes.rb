$stdout.sync = true

# Integer() with radix prefixes

# Hex
puts Integer("0xff")    # 255
puts Integer("0xFF")    # 255
puts Integer("0x1A")    # 26

# Binary
puts Integer("0b1010")  # 10
puts Integer("0b11111111")  # 255

# Octal — 0o prefix
puts Integer("0o17")    # 15
puts Integer("0o777")   # 511

# Octal — leading zero
puts Integer("017")     # 15

# Decimal
puts Integer("42")      # 42
puts Integer("-10")     # -10

# Integer still works for plain numbers
puts Integer(42)    # 42
puts Integer(3.9)   # 3 (truncates)

# String#to_i still uses decimal by default
puts "0xff".to_i    # 0
puts "0xff".to_i(16)  # 255
puts "1010".to_i(2)   # 10
