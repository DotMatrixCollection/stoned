scores = {alice: 85, bob: 92, carol: 78, dave: 71}
p scores.min_by { |_, v| v }
p scores.max_by { |_, v| v }
p scores.minmax_by { |_, v| v }
p scores.sort_by { |k, v| v }.map(&:first)
p scores.count { |_, v| v >= 80 }
