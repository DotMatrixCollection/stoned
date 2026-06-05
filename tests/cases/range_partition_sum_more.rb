r = (1..8)
p r.partition { |n| n.even? }
p r.select { |n| n > 4 }.sum
