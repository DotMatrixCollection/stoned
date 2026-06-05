m = /(\w+)-(\d+)-(\w+)/.match("abc-123-xyz")
p m[0]
p m[2]
p m.captures
p m.length
