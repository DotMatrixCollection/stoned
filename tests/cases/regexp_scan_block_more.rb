p "abc123def".scan(/\d/)
p "abc123def".scan(/([a-z]+)(\d+)/)
out = []
"a1 b2".scan(/([a-z])(\d)/) { |x, y| out << "#{x}:#{y}" }
p out
