h = { a: 1, b: 2 }
p h.fetch(:a)
p h.fetch(:z, 9)
p h.key?(:b)
p h.has_value?(2)
p h.to_a
