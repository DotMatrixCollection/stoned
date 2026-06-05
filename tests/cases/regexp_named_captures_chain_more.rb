m = /(?<name>[a-z]+)-(?<id>\d+)/.match("ruby-42")
p m[:name]
p m[:id]
p m.named_captures
