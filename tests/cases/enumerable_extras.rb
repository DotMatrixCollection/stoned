p [1, 2, 3, 4, 5].filter_map { |x| x * 10 if x.odd? }
p [1, 2, 3, 4, 5].each_slice(2).to_a
p [1, 2, 3, 4, 5].each_cons(3).to_a
p [1, 2, 3, 4, 5].take_while { |x| x < 4 }
p [1, 2, 3, 4, 5].drop_while { |x| x < 4 }
p (1..6).each_slice(2).to_a
p (1..5).each_cons(2).to_a
