p 1.step(7, 2).map { |n| n.divmod(2) }
p [-3, -2, -1, 0, 1].select(&:negative?).map(&:abs)
