h = {a: 1, b: 2, c: 3}
result = h.each_with_object([]) { |(k,v), arr| arr << "#{k}:#{v}" }
p result.sort

result2 = h.map { |k,v| [k, v * 2] }.to_h
p result2

result3 = h.flat_map { |k,v| [k, v] }
p result3
