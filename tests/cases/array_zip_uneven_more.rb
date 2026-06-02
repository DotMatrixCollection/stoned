left = [1, 2, 3]
right = ["a"]

p left.zip(right)

rows = []
left.zip(right, [:x, :y]) { |row| rows << row }
p rows
