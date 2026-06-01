h = {a: 1, b: 2, c: 3}
p h.fetch_values(:a, :c)
p h.values_at(:c, :missing, :a)
