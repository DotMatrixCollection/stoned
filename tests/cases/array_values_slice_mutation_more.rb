items = [["a"], ["b"], ["c"], ["d"]]

p items.values_at(0, -1)
p items[1, 2]
p items[-3..-2]

slice = items[1, 2]
slice[0] << "!"
p items
p slice
