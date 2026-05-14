"hello world" =~ /(\w+)\s(\w+)/
p $~[0]
p $~[1]
p $~[2]
p $1
p $2
p $3

"hello" =~ /xyz/
p $~
p $1

"foobar" =~ /o+/
p $~[0]
p $1
