puts (1..Float::INFINITY).lazy.select { |x| x.odd? }.first(5).inspect
puts (1..Float::INFINITY).lazy.map { |x| x * x }.first(4).inspect
puts (1..Float::INFINITY).lazy.select { |x| x % 3 == 0 }.map { |x| x * 2 }.first(3).inspect
puts [1,2,3,4,5].lazy.select { |x| x > 2 }.first(2).inspect
# to_a and take
puts [1,2,3].lazy.map { |x| x * 2 }.to_a.inspect
puts [1,2,3].lazy.select { |x| x > 1 }.to_a.inspect
puts (1..5).lazy.map { |x| x + 10 }.to_a.inspect
puts (1..Float::INFINITY).lazy.take(3).to_a.inspect
puts (1..Float::INFINITY).lazy.select { |x| x.odd? }.take(4).to_a.inspect
puts [1,2,3,4,5].lazy.take(3).first(2).inspect
