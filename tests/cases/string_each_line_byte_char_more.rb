text = "ab\ncd\n"

p text.lines
p text.each_line.map { |line| line.chomp }
p text.bytes.take(4)
p text.chars.select { |ch| ch != "\n" }
