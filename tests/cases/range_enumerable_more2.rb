p (1..3).map { |x| x * 10 }
p (1..3).select(&:odd?)
p (1..3).reduce(:+)
