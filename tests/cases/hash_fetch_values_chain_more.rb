h = { a: 1, b: 2, c: 3 }
p h.fetch_values(:c, :a)
p h.fetch(:z) { |key| key }
p h.values_at(:b, :missing)
