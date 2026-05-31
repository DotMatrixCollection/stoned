(a, b), c = [1, 2], 3
puts a
puts b
puts c

x, (y, z) = 10, [20, 30]
puts x
puts y
puts z

(a2, *b2), c2 = [1, 2, 3], 4
puts a2
puts b2.inspect
puts c2

a3, (b3, c3, d3) = 1, [2, 3, 4]
puts a3
puts b3
puts c3
puts d3
