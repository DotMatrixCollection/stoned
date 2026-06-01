h = {a: 1}
p h.fetch(:a)
p h.fetch(:b, 2)
p h.fetch(:c) { |k| k.to_s }
