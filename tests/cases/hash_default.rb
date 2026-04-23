plain = Hash.new
puts plain[:missing].inspect

fixed = Hash.new(7)
puts fixed[:missing]
fixed[:a] = 1
puts fixed[:a]
puts fixed[:other]

memo = Hash.new { |h, k| h[k] = k * 10 }
puts memo[3]
puts memo[3]
puts memo[4]
puts memo.inspect

copy = fixed.dup
puts copy[:z]
