h = {:a => 1}
h[:b] = 2
h.store(:c, 3)
puts h[:a]
puts h[:b]
puts h[:c]
puts h.length

copy = h.dup
copy[:a] = 9
puts h[:a]
puts copy[:a]

merged = h.merge({:d => 4})
puts h[:d]
puts merged[:d]
