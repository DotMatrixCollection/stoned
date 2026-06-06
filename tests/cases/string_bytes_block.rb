r = []
"abc".bytes { |b| r << b }
p r
p "abc".bytes { |b| b * 2 }
