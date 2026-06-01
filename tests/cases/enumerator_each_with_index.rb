e = ["a", "b"].each_with_index
p e.to_a
p e.map { |v, i| "#{i}:#{v}" }
