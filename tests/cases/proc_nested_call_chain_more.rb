double = proc { |n| n * 2 }
inc = proc { |n| n + 1 }
p [1, 2, 3].map { |n| inc.call(double.call(n)) }
p double.call(inc.call(4))
