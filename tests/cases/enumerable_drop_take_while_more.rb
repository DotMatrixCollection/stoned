a = [1, 2, 3, 1, 2]
p a.take_while { |n| n < 3 }
p a.drop_while { |n| n < 3 }
