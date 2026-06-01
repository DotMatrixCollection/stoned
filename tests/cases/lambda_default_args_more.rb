f = ->(a, b = 2) { a + b }
p f.call(3)
p f.call(3, 4)
