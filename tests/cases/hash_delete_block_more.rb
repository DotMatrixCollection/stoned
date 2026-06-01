h = {a: 1, b: 2}
p h.delete(:a)
p h.delete(:z) { |k| "missing #{k}" }
p h
