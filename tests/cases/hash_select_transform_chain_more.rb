h = { a: 1, b: 2, c: 3 }
p h.select { |k, v| v.odd? }
p h.transform_keys { |k| k.to_s.upcase }
