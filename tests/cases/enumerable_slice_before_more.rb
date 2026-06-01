p [1, 2, 5, 6, 9].slice_before { |n| n.odd? }.to_a
p ["a", "b", "aa"].slice_before { |s| s.length == 2 }.to_a
