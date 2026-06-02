# Hash#invert swaps keys and values
h = {a: 1, b: 2, c: 3}
p h.invert
p h.invert.invert

# invert with duplicate values — last one wins
dup = {a: 1, b: 1, c: 2}
p dup.invert

# Practical: reverse lookup
codes = {ok: 200, not_found: 404, error: 500}
by_code = codes.invert
p by_code[200]
p by_code[404]

# Hash set operations via merge patterns
defaults = {color: "red", size: "medium", weight: 1.0}
overrides = {color: "blue", weight: 2.5}
p defaults.merge(overrides)

# merge creating union-like result
a = {x: 1, y: 2}
b = {y: 3, z: 4}
p a.merge(b)
p b.merge(a)

# Hash subtraction via reject
full = {a: 1, b: 2, c: 3, d: 4}
keys_to_remove = [:b, :d]
p full.reject { |k, _| keys_to_remove.include?(k) }

# select as hash filter
p full.select { |_, v| v > 2 }

# transform_keys
p h.transform_keys(&:to_s)
p h.transform_keys { |k| k.to_s.upcase }
