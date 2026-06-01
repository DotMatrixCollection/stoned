p (1..20).lazy.select { |n| n.even? }.map { |n| n + 1 }.take(4).to_a
p [1, 2, 3].lazy.flat_map { |n| [n, n * 10] }.drop(2).take(3).to_a
