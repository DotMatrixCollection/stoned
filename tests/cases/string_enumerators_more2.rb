p "abc".bytes.map { |b| b + 1 }
p "abc".chars.map(&:upcase)
p "abc".each_char.to_a
