p [1,2,3].map { |n| n * 2 }.select { |n| n > 2 }.reduce(:+)
p (1..10).select(&:even?).map { |n| n ** 2 }.first(3)
p %w[cat elephant dog ant].sort_by(&:length).map(&:upcase)
