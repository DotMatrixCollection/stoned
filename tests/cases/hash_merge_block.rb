# Hash#merge with conflict resolution block
h1 = {a: 1, b: 2, c: 3}
h2 = {b: 20, c: 30, d: 40}

# Without block: h2 wins
p h1.merge(h2)

# With block: block decides
p h1.merge(h2) { |key, old, new_val| old + new_val }
p h1.merge(h2) { |key, old, new_val| [old, new_val] }

# merge! / update modifies receiver
h3 = {x: 1, y: 2}
h4 = {y: 20, z: 30}
h3.merge!(h4) { |k, o, n| o * n }
p h3

# Merge multiple hashes (Ruby 2.6+)
p({a: 1}.merge({b: 2}, {c: 3}))

# merge preserves key order
p({c: 3, a: 1}.merge({b: 2, a: 10}))

# Block receives key, old value, new value
keys_seen = []
{a: 1, b: 2}.merge({a: 10, b: 20}) { |k, o, n| keys_seen << k; o }
p keys_seen.sort

# update is alias for merge!
h5 = {one: 1}
h5.update({two: 2, three: 3})
p h5
