h = {a: 1, b: 2, c: 3}
p h.select { |k, v| v.odd? }.keys
p h.reject { |k, v| k == :b }.values
