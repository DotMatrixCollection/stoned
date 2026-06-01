require "set"

s = Set[1, 2, 3]
p s.delete(2).to_a.sort
p s.include?(2)
p s.add(4).to_a.sort
