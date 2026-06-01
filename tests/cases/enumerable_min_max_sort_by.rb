words = %w[banana cherry apple date elderberry]
p words.min_by(&:length)
p words.max_by(&:length)
p words.sort_by(&:length)
p words.min_by { |w| [w.length, w] }
p words.max_by { |w| [-w.length, w] }

nums = [-3, 1, -2, 4, -1]
p nums.min_by { |n| n.abs }
p nums.max_by { |n| n.abs }
p nums.sort_by { |n| n.abs }
