re = /(?<word>[a-z]+)-(?<num>\d+)/
m = re.match("abc-123")
p m.names
p m.named_captures
p m[:word]
