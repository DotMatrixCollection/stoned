p (1..10).each_slice(3).to_a
p (1..10).each_cons(4).to_a
p (1..5).sum
p (1..5).reduce(:+)
p (1..5).inject(10) { |acc, n| acc + n }
