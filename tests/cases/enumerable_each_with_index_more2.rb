result = []
["a", "b", "c"].each_with_index { |v, i| result << "#{i}:#{v}" }
p result

result2 = (1..5).each_with_index.map { |v, i| [i, v] }
p result2

result3 = %w[foo bar baz].each_with_index.select { |v, i| i.odd? }.map(&:first)
p result3
