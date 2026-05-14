p [1, 2, 3, 4].values_at(0, 2, -1)
p({a: 1, b: 2, c: 3}.values_at(:a, :c, :z))
p "hello".insert(2, "XY")
p "hello".insert(-1, "!")
p "hello".slice!(1, 3)
p "hello".slice!(0, 2)
p "hello world".match(/(\w+)\s(\w+)/).to_a
p "hello world".match(/(\w+)\s(\w+)/).captures
p "hello world".match(/(\w+)\s(\w+)/).length
