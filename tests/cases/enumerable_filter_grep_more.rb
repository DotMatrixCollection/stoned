p [1, 2, 3, 4].filter_map { |x| x * 2 if x.even? }
p [1, 2, 3].grep(2..3)
p [1, 2, 3].grep_v(2..3)
