h = { a: 1, b: nil, c: 3 }
p h.compact
p h.compact.transform_values { |v| v * 10 }
