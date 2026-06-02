text = "red=1 green=22 blue=333"

p text.scan(/(\w+)=(\d+)/)
p text.scan(/\d+/).map { |n| n.to_i }
p text.gsub(/(\w+)=(\d+)/, "\\1:<\\2>")

seen = []
text.scan(/(\w+)=(\d+)/) { |name, value| seen << "#{name}:#{value.length}" }
p seen
