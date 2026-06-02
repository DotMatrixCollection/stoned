p (1..9).step(3).to_a
p (1...9).step(3).map { |n| n + 1 }
p ("a".."d").to_a
p (3..6).each_with_index.map { |value, index| value * index }
