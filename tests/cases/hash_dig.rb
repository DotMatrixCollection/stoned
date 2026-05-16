h = {a: {b: {c: 42}}}
puts h.dig(:a, :b, :c)
puts h.dig(:a, :x).inspect
puts h.dig(:z).inspect
a = [[1, [2, 3]], [4, [5, 6]]]
puts a.dig(0, 1, 0)
puts a.dig(1, 1).inspect
