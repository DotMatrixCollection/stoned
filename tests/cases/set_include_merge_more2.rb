require "set"

s = Set.new([1])
s.merge([2, 3])
p s.include?(2)
p s.to_a
