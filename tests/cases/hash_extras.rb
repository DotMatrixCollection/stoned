h = {a: 1, b: 2, c: 3}
p h.slice(:a, :c)
p h.except(:b)
p h.to_a
p({a: 1, b: 2}.invert)
p({a: 1, b: 2}.key(1))
p({a: 1, b: 2}.key(99))
p h.assoc(:b)
p h.rassoc(3)
