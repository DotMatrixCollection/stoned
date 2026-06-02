# each_with_index provides index alongside element
["a", "b", "c"].each_with_index { |el, i| puts "#{i}: #{el}" }

# each_with_index returns enumerator
pairs = [10, 20, 30].each_with_index.to_a
p pairs

# map.with_index
p ["x", "y", "z"].map.with_index { |el, i| "#{i}:#{el}" }
p ["x", "y", "z"].map.with_index(1) { |el, i| "#{i}:#{el}" }

# each_with_index on hash
{a: 1, b: 2, c: 3}.each_with_index { |(k, v), i| puts "#{i}:#{k}=#{v}" }

# select with index via each_with_index
p (1..8).each_with_index.select { |n, i| i.even? }.map(&:first)

# Comparable index patterns
words = ["zero", "one", "two", "three", "four"]
p words.each_with_index.max_by { |w, _i| w.length }
p words.each_with_index.min_by { |_w, i| i }

# flat_map with index
p [10, 20, 30].each_with_index.flat_map { |n, i| [i, n] }
