h = {a: 1, b: 2}
p h.transform_values { |v| v * 10 }
p h.transform_values.to_a
