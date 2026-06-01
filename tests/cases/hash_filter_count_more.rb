h = {a: 1, b: 2, c: 3}
p h.filter_map { |k, v| "#{k}=#{v}" if v.odd? }
p h.count
p h.count { |k, v| v > 1 }
