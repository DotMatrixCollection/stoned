a = []
[1, 2, 3].reverse_each { |n| a << n * 10 }
p a
p ["a", "b"].reverse_each.map { |s| s.upcase }
