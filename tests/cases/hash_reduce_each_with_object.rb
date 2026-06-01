h = {a: 1, b: 2, c: 3}

inverted = h.each_with_object({}) { |(k,v), acc| acc[v] = k }
p inverted

total = h.reduce(0) { |sum, (k,v)| sum + v }
p total

keys_vals = h.reduce([[], []]) { |(ks, vs), (k, v)| [[*ks, k], [*vs, v]] }
p keys_vals
