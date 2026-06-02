p({a: 1, b: 2}.to_a.sort_by { |k,v| v }.reverse)
p({a: 1, b: 2, c: 3}.filter { |k,v| v.odd? })
p({a: 1, b: 2, c: 3}.count { |k,v| v > 1 })
p({a: 1, b: 2, c: 3}.sum { |k,v| v })
