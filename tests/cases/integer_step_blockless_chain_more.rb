enum = 2.step(10, 2)

p enum.to_a
p enum.map { |n| n * n }
p 5.downto(1).select { |n| n.odd? }
p 3.times.with_index.map { |value, idx| value + idx }
