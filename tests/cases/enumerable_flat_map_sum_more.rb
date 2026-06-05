rows = [[1, 2], [3], [4, 5]]
p rows.flat_map { |row| row.map { |n| n * 2 } }
p rows.flatten.sum
