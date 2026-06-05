double = proc { |n| n * 2 }
inc = proc { |n| n + 1 }
combo = proc { |n| double.call(inc.call(n)) }
p [1, 2, 3].map(&combo)
p combo.call(10)
