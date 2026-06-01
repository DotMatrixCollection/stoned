require "stringio"

io = StringIO.new("abcdef")
p io.read(2)
p io.pos
io.seek(1)
p io.read(3)
p io.eof?
