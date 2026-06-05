p [1, 2, 3, 4].each_cons(3).map { |w| w.reduce(:+) }
p [1, 2, 3, 4].each_slice(3).map { |w| w.join(":") }
