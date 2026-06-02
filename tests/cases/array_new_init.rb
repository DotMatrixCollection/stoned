# Array.new initialization forms
p Array.new(3)
p Array.new(3, "x")
p Array.new(5) { |i| i * 2 }
p Array.new(4) { |i| i.even? ? "even" : "odd" }

# Array() conversion
p Array(nil)
p Array(1)
p Array([1, 2, 3])
p Array({a: 1})

# Kernel#Array respects to_ary then to_a
p Array(1..4)

# fill
arr = Array.new(5, 0)
arr.fill(7, 2, 2)
p arr

arr2 = [0, 0, 0, 0, 0]
arr2.fill { |i| i ** 2 }
p arr2

arr3 = [1, 2, 3, 4, 5]
arr3.fill(99, 1..3)
p arr3

# Array multiplication
p [1, 2, 3] * 2
p [1, 2, 3] * ", "

# flatten with depth
nested = [1, [2, [3, [4]]]]
p nested.flatten
p nested.flatten(1)
p nested.flatten(2)
