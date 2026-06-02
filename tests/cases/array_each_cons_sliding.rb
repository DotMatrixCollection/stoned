nums = [1, 2, 3, 4, 5]
p nums.each_cons(2).to_a
p nums.each_cons(3).to_a
p nums.each_cons(2).map { |a, b| a + b }
p nums.each_cons(3).map { |triple| triple.sum / 3.0 }
