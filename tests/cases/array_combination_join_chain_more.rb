letters = ["a", "b", "c"]
p letters.combination(2).map { |pair| pair.join }
p letters.combination(3).map { |pair| pair.join("-") }
