require "zlib"
require "stringio"

io = StringIO.new("")
gz = Zlib::GzipWriter.new(io)
p gz.write("abc")
p (gz << "d").class
p gz.finish
