groups = [1,2,3,4,5,6].group_by { |n| n % 3 }
p groups.keys.sort
p groups[0].sort
p groups[1].sort

words = %w[ant bear cat duck eel]
by_len = words.group_by(&:length)
p by_len.keys.sort
p by_len[3].sort
