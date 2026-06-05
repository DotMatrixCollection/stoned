add = proc { |a, b| a + b }
p [[1, 2], [3, 4]].map { |pair| add.call(pair[0], pair[1]) }
p [1, 2, 3].map(&:to_s).join(",")
