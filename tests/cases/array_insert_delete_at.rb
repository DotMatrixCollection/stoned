arr = [1, 2, 3, 4, 5]
arr.insert(2, 99, 100)
puts arr.inspect

arr.delete(99)
puts arr.inspect

puts arr.delete_at(0)
puts arr.inspect

puts arr.delete_at(-1)
puts arr.inspect

arr2 = [1, 2, 3]
arr2.insert(-1, 4)
puts arr2.inspect

arr2.insert(0, 0)
puts arr2.inspect

puts arr2.delete_at(10).inspect
