h = { a: 1, b: 2, c: 3 }
p h.delete(:b)
p h
p h.fetch(:z) { |key| key.to_s.upcase }
