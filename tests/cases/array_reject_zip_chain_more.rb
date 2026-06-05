a = [1, 2, 3, 4]
p a.reject { |n| n.odd? }
p a.zip(["a", "b", "c", "d"]).map { |pair| pair.join(":") }
