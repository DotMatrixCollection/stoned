"abc123" =~ /([a-z]+)(\d+)/
p $~
p $1
p $2

"no digits" =~ /(\d+)/
p $~
p $1

md = /([A-Z]+)/.match("xxABCyy")
p md[1]
p $1
