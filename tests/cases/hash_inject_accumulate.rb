scores = {alice: 85, bob: 92, carol: 78}

total = scores.inject(0) { |sum, (_, v)| sum + v }
p total

doubled = scores.inject({}) { |h, (k, v)| h.merge(k => v * 2) }
p doubled

inverted = scores.each_with_object({}) { |(k, v), h| h[v] = k }
p inverted

p scores.sum { |_, v| v }
