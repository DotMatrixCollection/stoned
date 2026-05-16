result = [1, 2, 3].each_with_object([]) { |x, acc| acc << x * 2 }
p result
result2 = {a: 1, b: 2}.each_with_object({}) { |(k, v), acc| acc[k] = v * 10 }
p result2
