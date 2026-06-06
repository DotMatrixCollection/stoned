collected = []
5.times { |i| collected << i * i }
p collected
p collected.inject(:+)
p 4.times.map { |i| i + 1 }
p 3.times.inject(1) { |acc, _| acc * 2 }
