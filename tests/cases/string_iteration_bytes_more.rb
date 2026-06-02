p "abc".chars
p "abc".bytes
p "a\nb\n".lines
p "abc".each_char.map { |c| c.upcase }
p "abc".each_byte.map { |b| b + 1 }
