# Enumerable#sum
p [1, 2, 3, 4, 5].sum
p (1..100).sum
p [1.5, 2.5, 3.0].sum
p [].sum
p [].sum(10)

# sum with block transforms before summing
p ["apple", "banana", "fig"].sum(&:length)
p [1, 2, 3, 4].sum { |n| n * n }

# inject / reduce
p [1, 2, 3, 4, 5].inject(:+)
p [1, 2, 3, 4, 5].inject(10, :+)
p [1, 2, 3, 4, 5].inject { |sum, n| sum + n }
p [1, 2, 3, 4, 5].reduce(1) { |prod, n| prod * n }

# inject to build structures
p [1, 2, 3].inject([]) { |acc, n| acc + [n, n * 2] }
p %w[cat dog bird].inject({}) { |h, w| h.merge(w => w.length) }

# each_with_object vs inject
result = [1, 2, 3].each_with_object([]) { |n, a| a << n * 3 }
p result

# Enumerable#sum with initial value
p [[1, 2], [3, 4]].sum([]) { |a| a }
