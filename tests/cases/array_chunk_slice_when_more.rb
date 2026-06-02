p [1,2,3,4,5,6].chunk { |n| n < 4 }.to_a
p [1,1,2,2,3,1,1].chunk_while { |a,b| a == b }.to_a
p [1,2,3,4,5].slice_when { |a,b| b - a > 1 }.to_a
