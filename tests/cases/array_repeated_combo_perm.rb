$stdout.sync = true

# Array#repeated_combination — k-length combinations with repetition allowed

puts [1, 2, 3].repeated_combination(1).to_a.inspect
# [[1], [2], [3]]

puts [1, 2].repeated_combination(2).to_a.inspect
# [[1, 1], [1, 2], [2, 2]]

puts [1, 2].repeated_combination(3).to_a.inspect
# [[1,1,1],[1,1,2],[1,2,2],[2,2,2]]

puts [1, 2].repeated_combination(0).to_a.inspect
# [[]]

# Array#repeated_permutation — k-length sequences with repetition

puts [1, 2].repeated_permutation(1).to_a.inspect
# [[1],[2]]

puts [1, 2].repeated_permutation(2).to_a.inspect
# [[1,1],[1,2],[2,1],[2,2]]

puts [1, 2].repeated_permutation(0).to_a.inspect
# [[]]

# Both support block form (returns receiver)
combos = []
result = [1, 2].repeated_combination(2) { |c| combos << c.inspect }
puts combos.join(" ")
puts result.inspect  # [1, 2]
