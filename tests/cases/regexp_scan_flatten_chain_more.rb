text = "a1 b22 c333"
p text.scan(/([a-z])(\d+)/).flatten
p text.scan(/([a-z])(\d+)/).map { |pair| pair.reverse.join }
