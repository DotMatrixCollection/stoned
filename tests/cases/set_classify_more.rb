require "set"

s = Set.new([1, 2, 3, 4])
p s.select { |n| n.even? }.to_a
p s.map { |n| n * 2 }
