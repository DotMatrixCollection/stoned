p [1, 2, 3, 4] - [2, 4]
p [1, 2, 2, 3] & [2, 3, 4]
p [1, 2] | [2, 3, 4]
p [1, 2] * 3
p [1, 2, 3].combination(2).to_a
p [1, 2, 3].permutation(2).to_a
p [1, 2].product([3, 4])
p [1, 2].product([3, 4], [5])
[1, 2, 3].combination(2) { |c| p c }
