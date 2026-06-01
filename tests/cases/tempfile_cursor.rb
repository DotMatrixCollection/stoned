require "tempfile"

t = Tempfile.new("cursor")
t.write("abc\ndef")
t.flush
p t.tell > 0
t.rewind
p t.getc
p t.getbyte
p t.pos
t.close!
p File.exist?(t.path)
