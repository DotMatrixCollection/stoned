require "stringio"

io = StringIO.new("a\nb\n")
p io.gets
p io.readline
begin
  io.readline
rescue EOFError => e
  puts e.class
end
