# Hash to array conversions and flattening
h = {a: 1, b: 2, c: 3}
p h.to_a
p h.to_a.sort

# flatten on hash#to_a
p h.to_a.flatten
p h.to_a.flatten(1)

# Nested hash to_a
nested = {x: {y: 1}, z: [2, 3]}
p nested.to_a

# Hash.[] from array of pairs
pairs = [[:a, 1], [:b, 2], [:c, 3]]
p Hash[pairs]
p pairs.to_h

# to_h with block (Ruby 2.6+)
p [[:a, 1], [:b, 2]].to_h { |k, v| [k, v * 10] }

# assoc returns [key, value] pair
p h.assoc(:b)
p h.assoc(:z)

# rassoc finds by value
p h.rassoc(2)
p h.rassoc(99)

# flatten with nested arrays in values
hv = {a: [1, [2, 3]], b: [4, 5]}
p hv.to_a.flatten
p hv.to_a.flatten(2)
