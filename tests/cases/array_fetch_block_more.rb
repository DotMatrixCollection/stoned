p [1, 2, 3].fetch(1)
p [1, 2, 3].fetch(9, :missing)
p [1, 2, 3].fetch(9) { |i| i * 2 }
