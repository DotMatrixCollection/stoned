a = {x: 1, y: 2}
b = {y: 10, z: 3}
p a.merge(b)
p a.merge(b) { |k, v1, v2| v1 + v2 }
a.merge!(b)
p a
p({}.merge(x: 1).merge(y: 2))
