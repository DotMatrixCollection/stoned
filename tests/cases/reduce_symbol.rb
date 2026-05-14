p [1, 2, 3, 4].reduce(:+)
p [1, 2, 3, 4].inject(:+)
p [1, 2, 3, 4].reduce(10, :+)
p (1..5).reduce(:+)
p (1..5).inject(100, :+)
p ["a", "b", "c"].inject(:+)
p [1, 2, 3].reduce { |s, x| s + x }
