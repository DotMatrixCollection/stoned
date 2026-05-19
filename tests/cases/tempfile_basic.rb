require 'tempfile'

Tempfile.create("test") do |f|
  f.write("content here")
  f.flush
  p File.read(f.path)
  p File.exist?(f.path)
end

t = Tempfile.new("mytest")
t.write("hello world")
t.close
p File.read(t.path)
t.unlink
p File.exist?(t.path)
