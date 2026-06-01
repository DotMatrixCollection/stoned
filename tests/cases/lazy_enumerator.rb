puts (1..Float::INFINITY).lazy.select { |n| n.odd? }.first(4).inspect
puts (1..Float::INFINITY).lazy.map { |n| n * n }.first(4).inspect
puts (1..Float::INFINITY).lazy.take_while { |n| n < 5 }.to_a.inspect
puts (1..Float::INFINITY).lazy.drop(5).first(3).inspect
puts (1..Float::INFINITY).lazy.drop_while { |n| n < 10 }.first(3).inspect
puts (1..Float::INFINITY).lazy.flat_map { |n| [n, n*10] }.first(6).inspect
puts [1,2,3,4,5].lazy.select { |n| n.even? }.map { |n| n*3 }.to_a.inspect
