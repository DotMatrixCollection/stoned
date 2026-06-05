p [0, 1, 2, 3].map { |n| [n, n.zero?, n.positive?] }
p [-2, -1, 0, 1].select { |n| n.negative? }
