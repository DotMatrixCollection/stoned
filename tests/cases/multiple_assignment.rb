a, b = 1, 2
puts a
puts b

a, b = b, a
puts a
puts b

x, y = [3, 4]
puts x
puts y

left, right = 9
puts left
p right

head, *rest = [5, 6, 7]
puts head
p rest

first, *middle, last = [8, 9, 10, 11]
puts first
p middle
puts last

outer, (inner_a, inner_b) = [12, [13, 14]]
puts outer
puts inner_a
puts inner_b

def pair_sum((a, b))
  a + b
end

puts pair_sum([20, 22])

[[1, 2], [3, 4]].each do |(a, b)|
  puts a + b
end
