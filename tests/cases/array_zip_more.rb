# Array#zip combines arrays element-wise
p [1, 2, 3].zip([4, 5, 6])
p [1, 2, 3].zip([4, 5, 6], [7, 8, 9])
p [:a, :b, :c].zip([1, 2, 3])

# Short arrays padded with nil
p [1, 2, 3].zip([4, 5])
p [1, 2].zip([4, 5, 6])

# zip with block
result = []
[1, 2, 3].zip([10, 20, 30]) { |pair| result << pair.sum }
p result

# Transpose (inverse of zip)
p [[1, 4], [2, 5], [3, 6]].transpose
p [[1, 2, 3], [4, 5, 6]].transpose

# zip for parallel iteration
names  = ["Alice", "Bob", "Carol"]
scores = [95, 87, 91]
grades = ["A", "B", "A"]
names.zip(scores, grades).each do |name, score, grade|
  puts "#{name}: #{score} (#{grade})"
end

# Unzip via transpose
pairs = [[1, "a"], [2, "b"], [3, "c"]]
nums, letters = pairs.transpose
p nums
p letters
