h = {a: 1, b: 2, c: 3}
p h.any? { |k, v| v > 2 }
p h.all? { |k, v| v > 0 }
p h.none? { |k, v| v > 10 }
p h.count { |k, v| v.odd? }
p h.min_by { |k, v| v }
