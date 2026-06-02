p [1, 2].product(["a", "b"])

collected = []
[1, 2, 3].combination(2) { |pair| collected << pair.join("-") }
p collected

p [1, 2, 3].permutation(2).select { |a, b| a < b }
