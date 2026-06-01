require "set"

s = Set.new([1, 2])
p s.include?(1)
s.add(3)
p s.to_a.sort
p((s | Set.new([3, 4])).to_a.sort)
p((s & Set.new([2, 4])).to_a.sort)
