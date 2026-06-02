match = /(?<word>[a-z]+)-(?<num>\d+)/.match("abc-123")

p match[:word]
p match[:num]
p match.to_a
p match.captures
p match.names
p match.named_captures
