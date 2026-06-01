p "one two three".scan(/\w+/)
p "one1 two2 three3".scan(/([a-z]+)(\d+)/)

"hello" =~ /h(e)/
p $1

"abc123" =~ /([a-z]+)(\d+)/
p $1
p $2
p $&
p $`
p $'
