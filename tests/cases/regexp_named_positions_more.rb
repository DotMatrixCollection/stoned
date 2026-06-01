r = /(?<word>\w+)-(\d+)/
m = r.match("abc-123")
p m.names
p m.named_captures
p m.begin(0)
p m.end(0)
p m.pre_match
p m.post_match
