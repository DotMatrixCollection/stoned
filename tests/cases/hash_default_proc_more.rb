h = Hash.new { |hash, key| hash[key] = key.to_s.upcase }
p h[:x]
p h
