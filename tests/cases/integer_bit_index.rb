$stdout.sync = true

# Integer#[] — bit indexing (LSB = 0)

# 10 in binary: 1010
puts 10[0]   # 0
puts 10[1]   # 1
puts 10[2]   # 0
puts 10[3]   # 1
puts 10[4]   # 0  (beyond the number)
puts 10[-1]  # 0  (negative index → 0)

# 255 in binary: 11111111
puts (0..7).map { |i| 255[i] }.join(" ")

# 0 has all zero bits
puts 0[0]   # 0
puts 0[63]  # 0

# Works with send
puts 10.send(:[], 1)  # 1
