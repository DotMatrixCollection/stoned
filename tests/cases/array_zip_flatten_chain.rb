a = [1, 2, 3]
b = ["a", "b", "c"]
p a.zip(b)
p a.zip(b).flatten
p a.zip(b).map { |pair| pair.join(":") }
p [1, 2].zip([3, 4], [5, 6])
