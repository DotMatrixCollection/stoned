require "stringio"

io = StringIO.new("abc")
puts io.read(1)
io.write("Z")
io.rewind
puts io.read
