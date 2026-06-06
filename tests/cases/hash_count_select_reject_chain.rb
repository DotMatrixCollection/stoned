scores = { alice: 85, bob: 42, carol: 91, dave: 60 }
p scores.count { |_, v| v >= 60 }
p scores.select { |_, v| v >= 60 }.keys
p scores.reject { |_, v| v >= 60 }.keys
p scores.select { |_, v| v > 80 }.map { |k, v| "#{k}:#{v}" }
