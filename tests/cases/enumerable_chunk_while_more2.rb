p [1, 2, 4, 5, 7].chunk_while { |a, b| b == a + 1 }.to_a
p [1, 2, 4, 5, 7].slice_when { |a, b| b > a + 1 }.to_a
