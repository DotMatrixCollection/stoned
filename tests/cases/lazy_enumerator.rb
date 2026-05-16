puts (1..Float::INFINITY).lazy.select { |x| x.odd? }.first(5).inspect
puts (1..Float::INFINITY).lazy.map { |x| x * x }.first(4).inspect
puts (1..Float::INFINITY).lazy.select { |x| x % 3 == 0 }.map { |x| x * 2 }.first(3).inspect
puts [1,2,3,4,5].lazy.select { |x| x > 2 }.first(2).inspect
