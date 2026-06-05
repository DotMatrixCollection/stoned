h = { a: 1, b: 2, c: 3 }
p h.values_at(:c, :a, :z)
p h.keys.reverse
p h.values.map { |v| v * 2 }
