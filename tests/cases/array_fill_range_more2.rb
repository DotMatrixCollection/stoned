a = [0, 0, 0, 0]
p a.fill(:x, 1, 2)
p a.fill { |i| i }
