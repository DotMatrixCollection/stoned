items = [1, 2, 3, 4, 5]

p items.take(3)
p items.drop(2)
p items.take_while { |n| n < 4 }
p items.drop_while { |n| n < 3 }
p items.drop(1).take(3).select { |n| n.even? }
