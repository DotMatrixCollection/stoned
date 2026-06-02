words = %w[hi hey hello bye]
by_length = words.group_by(&:length)
p by_length
p by_length.transform_values { |ws| ws.map(&:upcase) }
p by_length.transform_values(&:length)
p words.tally
