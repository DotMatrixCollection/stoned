keys = [:a, :b, :c]
vals = [1, 2, 3]
p keys.zip(vals)
p keys.zip(vals).to_h

p %w[x y z].zip([10, 20, 30]).map { |k, v| "#{k}:#{v}" }
p [1, 2, 3].zip([4, 5, 6], [7, 8, 9])
