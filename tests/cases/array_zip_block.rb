result = []
[1, 2, 3].zip([4, 5, 6]) { |pair| result << pair.sum }
puts result.inspect

[1, 2, 3].zip([4, 5, 6], [7, 8, 9]) { |pair| puts pair.inspect }

ret = [1, 2].zip([3, 4]) { |a| a }
puts ret.inspect
