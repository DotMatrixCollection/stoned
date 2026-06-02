p [1,2,3,4,5,6].each_slice(2).map { |s| s.sum }
p [1,2,3,4,5,6].each_cons(3).map { |w| w.sum }
p (1..10).lazy.map { |n| n * 2 }.select { |n| n > 10 }.first(3)
p [*1..5].zip([*6..10]).map { |a, b| a + b }
