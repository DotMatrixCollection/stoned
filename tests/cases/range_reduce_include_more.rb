r = (2..6)
p r.reduce(0) { |sum, n| sum + n }
p r.include?(4)
p r.include?(8)
