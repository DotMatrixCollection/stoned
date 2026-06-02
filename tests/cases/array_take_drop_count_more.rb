p [1,2,3,4,5].take_while { |n| n < 4 }
p [1,2,3,4,5].drop_while { |n| n < 4 }
p [1,2,1,3,1].count(1)
p [1,2,3,4,5].sum { |n| n.odd? ? n : 0 }
p [1,2,3].product([4,5]).map { |a,b| a * b }
