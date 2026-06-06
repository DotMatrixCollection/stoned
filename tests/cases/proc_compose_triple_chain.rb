double = ->(x) { x * 2 }
increment = ->(x) { x + 1 }
square = ->(x) { x * x }
f = double >> increment >> square
p f.call(3)
g = square << increment << double
p g.call(3)
p [1, 2, 3, 4].map(&(double >> increment))
