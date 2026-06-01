require "zlib"
require "stringio"

io = StringIO.new("line1\nline2\n")
gz = Zlib::GzipReader.new(io)
out = []
gz.each_line { |line| out << line.chomp }
p out
p gz.close.class
