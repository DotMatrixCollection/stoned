hash = {a: 1, b: nil}

p hash.fetch(:a)
p hash.fetch(:b, "fallback")
p hash.fetch(:missing, "fallback")
p hash.fetch(:missing) { |key| "block:#{key}" }
p hash.values_at(:a, :missing, :b)
