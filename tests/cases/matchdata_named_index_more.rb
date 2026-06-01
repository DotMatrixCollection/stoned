m = /(?<a>a)(b)/.match("ab")
p m[0]
p m[:a]
p m.names
