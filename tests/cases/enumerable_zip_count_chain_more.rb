letters = ["a", "b", "c"]
nums = [1, 2, 3]
p letters.zip(nums).map { |pair| pair.join }
p letters.zip(nums).count { |pair| pair[1] > 1 }
