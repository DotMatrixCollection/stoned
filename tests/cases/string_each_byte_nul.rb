s = "a\0c".b
p s.each_byte.to_a
p s.each_byte.map { |b| b + 1 }
seen = []
s.each_byte { |b| seen << b }
p seen
