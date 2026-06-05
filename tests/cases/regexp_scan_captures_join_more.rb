text = "x=1 y=22 z=333"
p text.scan(/([a-z])=(\d+)/)
p text.scan(/([a-z])=(\d+)/).map { |pair| pair.join(":") }
