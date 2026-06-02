p [1,2,3].map.with_index { |x, i| [i, x] }
p [1,2,3].map.with_index(10) { |x, i| [i, x] }
p %w[a b c].each_with_index.map { |x, i| "#{i}:#{x}" }
p (1..5).lazy.select { |n| n.odd? }.first(3)
