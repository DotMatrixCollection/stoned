text = "red, green;blue"
p text.split(/[,;] */)
p text.match(/green/)[0]
p text.gsub(/(red|blue)/) { |m| m.upcase }
