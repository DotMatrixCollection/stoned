pairs = [[:a, 1], [:b, 2]]
p pairs.product([:x, :y]).map { |left, right| [left[0], left[1], right] }
p [[1, 2], [3, [4, 5]]].flatten(2)
