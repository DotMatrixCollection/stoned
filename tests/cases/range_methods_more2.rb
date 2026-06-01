p (1..10).include?(5)
p (1..10).include?(11)
p (1...10).include?(10)
p (1...10).include?(9)
p ("a".."z").include?("m")
p ("a".."z").include?("A")

p (1..5).reduce(:+)
p (1..5).map { |n| n ** 2 }
p (1..10).select(&:odd?)
p (1..10).reject(&:even?)
p (1..5).to_a.inject(1) { |acc, n| acc * n }
