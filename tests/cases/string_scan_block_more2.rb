out = []
ret = "a1 b2".scan(/([a-z])(\d)/) { |letter, digit| out << "#{letter}:#{digit}" }
p ret
p out
