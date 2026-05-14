p [1, 2, 3, 4, 5].partition { |x| x.odd? }
p ["apple", "pear", "plum", "fig"].partition { |w| w.length > 3 }
p [1, 2, 3].cycle(2).to_a
p [1, 2].cycle(3).to_a
