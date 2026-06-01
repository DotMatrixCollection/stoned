require "strscan"

s = StringScanner.new("abc 123")
p s.string
p s.pos
p s.peek(3)
p s.scan(/abc/)
p s.pos
p s.scan(/\s+/)
p s.scan(/\d+/)
p s.eos?
