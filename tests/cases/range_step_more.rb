p (1..5).step(2).to_a
out = []
(1..5).step(2) { |x| out << x }
p out
