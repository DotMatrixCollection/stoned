result = <<~TEXT.chomp
  hello world
TEXT
p result

lines = <<~TEXT.split("\n").length
  one
  two
  three
TEXT
p lines

upper = <<~TEXT.strip.upcase
  hello
TEXT
p upper
