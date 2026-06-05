p "abc123def45".scan(/[a-z]+|\d+/).map { |s| s.reverse }
p "a1 b22 c333".gsub(/([a-z])(\d+)/) { $2 + $1.upcase }
