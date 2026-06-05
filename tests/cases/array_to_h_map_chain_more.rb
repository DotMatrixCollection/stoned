items = [["a", 1], ["b", 2], ["c", 3]]
h = items.to_h
p h
p h.map { |k, v| k + v.to_s }
