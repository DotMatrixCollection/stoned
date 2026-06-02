p (1..9).step(3).to_a
p (1...5).map { |n| n * n }
p ("a".."d").reverse_each.to_a
p (1..5).each_with_index.map { |value, index| value + index }
