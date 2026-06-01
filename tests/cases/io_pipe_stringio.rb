rd, wr = IO.pipe
wr.write("hello")
wr.close
p rd.read
rd.close

require "stringio"
buf = StringIO.new
buf.write("hello ")
buf.write("world")
p buf.string

buf2 = StringIO.new
buf2.puts("line 1")
buf2.puts("line 2")
buf2.rewind
p buf2.gets
p buf2.gets
