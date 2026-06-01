p 3.times.to_a
p 1.upto(5).to_a
p 5.downto(1).to_a
p 1.step(10, 3).to_a
p [1, 2, 3].each_with_index.map { |x, i| "#{i}:#{x}" }
p(1.upto(5).select { |x| x.odd? })
p 3.times.map { |i| i * i }
p (1..4).each_with_index.map { |x, i| [i, x] }
h = {a: 1, b: 2}
p h.each_key.to_a
p h.each_value.map { |v| v * 10 }
p [1, 1, 2].chunk.class
p [1, 1, 2].chunk.to_a
p ("a".."c").each.class
p ("a".."c").each.to_a
