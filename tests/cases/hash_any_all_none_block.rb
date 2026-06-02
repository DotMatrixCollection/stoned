h = {a: 1, b: 2, c: 3}

p h.any? { |k, v| v > 2 }
p h.any? { |k, v| v > 10 }
p h.all? { |k, v| v > 0 }
p h.all? { |k, v| v > 1 }
p h.none? { |k, v| v > 5 }
p h.none? { |k, v| v > 2 }

p h.count { |k, v| v.even? }
p h.sum { |k, v| v }

# Key-based predicates
p h.any? { |k, _| k == :b }
p h.all? { |k, _| k.is_a?(Symbol) }
p h.none? { |k, _| k == :z }

# Empty hash
p({}.any? { true })
p({}.all? { false })
p({}.none? { true })
