add = ->(a, b) { a + b }
add5 = add.curry.(5)
p add5.(3)
p add5.(10)

mult = ->(a, b) { a * b }
double = mult.curry.(2)
p double.(7)

f = ->(x) { x + 1 }
g = ->(x) { x * 2 }
h = f >> g
p h.(3)

i = f << g
p i.(3)
