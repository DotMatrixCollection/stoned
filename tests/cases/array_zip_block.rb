$stdout.sync = true

# Array#zip with block calls block for each pair and returns nil

result = [1, 2, 3].zip([4, 5, 6]) { |a, b| puts "#{a}+#{b}=#{a+b}" }
puts result.nil?  # true (block form returns nil)

# zip without block returns array of arrays
pairs = [1, 2, 3].zip([4, 5, 6])
puts pairs.inspect  # [[1,4],[2,5],[3,6]]

# zip with multiple arrays and block
[1, 2].zip([3, 4], [5, 6]) { |a, b, c| puts "#{a},#{b},#{c}" }

# zip beyond end of other arrays pads with nil
puts [1, 2, 3].zip([4, 5]).inspect  # [[1,4],[2,5],[3,nil]]
