h1 = {a: 1, b: 2, c: 3}
h2 = {b: 20, c: 30, d: 40}

merged = h1.merge(h2)
p merged
p h1

h1.merge!(h2) { |key, v1, v2| v1 + v2 }
p h1

h3 = {x: 1}
h4 = {x: 2}
h5 = {x: 3}
p h3.merge(h4, h5)
