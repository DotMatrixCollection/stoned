words = %w[ant ape bat bear cat]

p words.group_by { |word| word[0] }
p words.map { |word| word.length }.tally
p words.select { |word| word.include?("a") }.map(&:upcase)
