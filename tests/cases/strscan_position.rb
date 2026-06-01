require "strscan"

s = StringScanner.new("xy")
p s.getch
p s.get_byte
p s.getch
p s.eos?
s.reset
p s.rest
p s.rest_size
s.pos = 1
p s.rest
