h = Hash.new(0)
%w[cat dog cat bird dog cat].each { |w| h[w] += 1 }
p h.sort_by { |k,v| -v }.to_h
p h.max_by { |k,v| v }
p h.sum { |k,v| v }
p h.any? { |k,v| v > 2 }
