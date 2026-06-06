words = ["apple", "ant", "banana", "berry", "cherry", "apricot"]
p words.filter_map { |w| w[0] if w.length > 4 }
p words.map { |w| w[0] }.tally
p words.filter_map { |w| w.upcase if w.start_with?("a") }
