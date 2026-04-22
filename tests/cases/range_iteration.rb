(1..5).each { |i| puts i }

r = 1...4
p r.to_a

(1..10).step(3) { |i| print "#{i} " }
puts

(0..4).each_with_index { |v, i| puts "#{i}:#{v}" }

mapped = (1..5).map { |i| i * 2 }
p mapped

evens = (1..6).select { |i| i.even? }
p evens
