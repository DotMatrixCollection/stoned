# Array min, max, minmax
p [3, 1, 4, 1, 5, 9, 2, 6].min
p [3, 1, 4, 1, 5, 9, 2, 6].max
p [3, 1, 4, 1, 5, 9, 2, 6].minmax
p [3, 1, 4, 1, 5, 9, 2, 6].min(3)
p [3, 1, 4, 1, 5, 9, 2, 6].max(3)

# min_by / max_by / minmax_by
words = ["apple", "fig", "banana", "kiwi", "elderberry"]
p words.min_by(&:length)
p words.max_by(&:length)
p words.minmax_by(&:length)
p words.min_by(2, &:length)
p words.max_by(2, &:length)

# sort with custom comparator
p [3, 1, 4, 1, 5].sort { |a, b| b <=> a }

# min/max on empty raises
begin
  [].min_by { |x| x }
rescue => e
  p e
end
p [].min
p [].max

# min/max with block as comparator
p ["banana", "Apple", "cherry"].min { |a, b| a.downcase <=> b.downcase }
p ["banana", "Apple", "cherry"].max { |a, b| a.downcase <=> b.downcase }
