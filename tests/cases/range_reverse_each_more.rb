out = []
(1..3).reverse_each { |n| out << n }
p out
p (1...3).reverse_each.to_a
