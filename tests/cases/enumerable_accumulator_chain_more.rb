p %w[ant ape bat].each_with_object(Hash.new(0)) { |word, counts| counts[word[0]] += 1 }

total = [1, 2, 3, 4].reduce(10) { |sum, n| sum + n }
p total

p [[1, 2], [3, 4]].flat_map { |pair| pair.map { |n| n * 2 } }
