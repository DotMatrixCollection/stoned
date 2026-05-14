p [[:a, 1], [:b, 2]].to_h
p [1, 2, 3].to_h { |x| [x, x * 2] }
p({a: 1, b: 2}.map { |k, v| [k, v * 10] }.to_h)
p({a: 1, b: 2}.to_h { |k, v| [k.to_s, v] })
p [1, 2, 3].map.with_index { |x, i| "#{i}:#{x}" }
p [1, 2, 3].map.with_index(1) { |x, i| [i, x] }
p ["a", "b"].each_with_index.map { |x, i| "#{i}:#{x}" }
