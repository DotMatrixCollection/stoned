words = %w[hello world hello ruby world hello]
freq  = words.tally
p freq
p freq.sort_by { |w,c| -c }.first(2)
p freq.max_by { |w,c| c }.first
p words.uniq.sort
