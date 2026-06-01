a = [:x, :y]
p a.fetch(1)
p a.fetch(9, :missing)
p a.fetch(9) { |i| "idx#{i}" }
