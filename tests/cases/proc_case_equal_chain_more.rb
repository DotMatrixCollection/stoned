even = proc { |n| n.even? }
p even === 4
p even === 5
p [1, 2, 3, 4].select(&even)
