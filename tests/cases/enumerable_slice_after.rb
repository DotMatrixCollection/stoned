p [1, 2, 3, 4, 5, 6].slice_after { |n| n.even? }.to_a
p [1, 2, 3, 0, 4, 5, 0, 6].slice_after(0).to_a
p [3, 1, 4, 1, 5, 9, 2, 6].slice_after { |n| n > 5 }.to_a
