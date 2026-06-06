nums = [1, 2, 3, 4, 5, 6, 7, 8]
grouped = nums.group_by { |n| n % 3 }
p grouped.keys.sort
p grouped[0]
p grouped.map { |k, v| [k, v.sum] }.sort_by { |k, _| k }
