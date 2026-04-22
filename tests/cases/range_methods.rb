r = 1..10
puts r.include?(5)
puts r.include?(10)
puts r.include?(11)

r2 = 1...10
puts r2.include?(10)
puts r2.include?(9)

r3 = 1..5
puts r3.min
puts r3.max

r4 = 1...5
puts r4.max

puts r3.size
puts r4.size
puts r3.sum

p r3.first(3)
p r3.last(2)

puts r3.reduce(0) { |acc, i| acc + i }

puts r === 7
puts r === 11

puts r.respond_to?(:each)
puts r.respond_to?(:include?)
