require "stringio"

io = StringIO.new("abc")
io.seek(1)
io.write("Z")
io.rewind
p io.read
