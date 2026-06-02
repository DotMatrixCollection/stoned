text = "one\ntwo\nthree\n"
p text.lines
p text.lines(chomp: true)

p "a,b,c".lines(",")

para = "hello\nworld\n\nfoo\n"
p para.lines("").length
p para.lines("").first.include?("hello")
