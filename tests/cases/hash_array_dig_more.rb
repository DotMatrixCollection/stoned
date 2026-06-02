h = {a: {b: {c: 42}}}
p h.dig(:a, :b, :c)
p h.dig(:a, :b)
p h.dig(:a, :x)
a = [[1, [2, 3]], [4, [5, 6]]]
p a.dig(0, 1, 0)
p a.dig(1, 1)
