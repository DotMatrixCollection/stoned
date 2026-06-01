a = [1, 2, 3]
p a.take(0)
p a.drop(5)
p a.take_while { |n| n < 3 }
p a.drop_while { |n| n < 3 }
