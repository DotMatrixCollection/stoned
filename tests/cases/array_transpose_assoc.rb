$stdout.sync = true

# Array#transpose
puts [[1,2],[3,4],[5,6]].transpose.inspect   # [[1,3,5],[2,4,6]]
puts [[1,2,3],[4,5,6]].transpose.inspect     # [[1,4],[2,5],[3,6]]
puts [].transpose.inspect                    # []

# Array#assoc / Array#rassoc
a = [[:a, 1], [:b, 2], [:c, 3]]
puts a.assoc(:b).inspect    # [:b, 2]
puts a.assoc(:z).inspect    # nil
puts a.rassoc(3).inspect    # [:c, 3]
puts a.rassoc(99).inspect   # nil

# String#gsub with hash replacement
puts "hello".gsub(/[aeiou]/, "e" => "3", "o" => "0")  # h3ll0
puts "banana".gsub(/[aeiou]/, "a" => "@")             # b@n@n@
puts "hello".sub(/[aeiou]/, "e" => "X")                # hXllo

# String#gsub plain string with hash
puts "aababc".gsub("a", "a" => "X")  # XXbXbc
puts "aababc".sub("a", "a" => "X")   # Xababc
