letters = [:a, :b, :c]

p letters.combination(2).to_a
p letters.permutation(2).to_a.first(3)
p [1, 2].product(["x", "y"])

result = []
[1, 2].product([10, 20]) { |left, right| result << left + right }
p result
