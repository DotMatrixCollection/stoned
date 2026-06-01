h = {a: 1, b: 2}
p h.merge({b: 20, c: 3}) { |k, old, new| "#{k}:#{old + new}" }
p h
