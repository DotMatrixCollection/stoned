p [3, 1, 4, 1, 5].minmax
p [3, 1, 4, 1, 5].minmax_by { |x| -x }
p ["banana", "apple", "cherry"].minmax
p({a: 1, b: 2}.none? { |k, v| v > 5 })
p({a: 1, b: 2}.none? { |k, v| v > 0 })
p({a: 1, b: 2}.any? { |k, v| v > 1 })
p({a: 1, b: 2}.all? { |k, v| v > 0 })
p({}.none?)
p({a: 1}.any?)
