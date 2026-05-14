h = {a: 1, b: 2, c: 3}
p h.transform_values { |v| v * 10 }
p h.transform_keys { |k| k.to_s }
p h.count
p h.count { |k, v| v > 1 }
p h.filter_map { |k, v| [k, v * 2] if v > 1 }

h2 = {x: 1, y: 2}
h2.transform_values! { |v| v + 100 }
p h2
h2.transform_keys! { |k| k.to_s }
p h2
