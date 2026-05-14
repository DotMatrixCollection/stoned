p [1, 2, 3].index(2)
p [1, 2, 3, 2].rindex(2)
p [1, 2, 3].find_index { |x| x > 1 }
p [1, 2, 3].index(99)
p [1, 2, 3].find_index { |x| x > 10 }
a = [1, 2, 3]
p a.shuffle.sort
p a.sample.class
