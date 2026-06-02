h = {a: [1, 2], b: [3, 4]}
p h.to_a
p h.flatten
p h.flatten(2)
p({x: 1, y: 2}.to_a.flatten)
p({p: [1, [2, 3]], q: [4]}.flatten(2))
