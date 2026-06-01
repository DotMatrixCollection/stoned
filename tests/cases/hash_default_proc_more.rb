h = Hash.new { |hash, key| hash[key] = key.to_s.upcase }
p h[:a]
p h
p h.default_proc.class
