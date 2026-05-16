require "pathname"

p = Pathname.new("/tmp/demo/file.txt")
puts p.to_s
puts p.dirname.to_s
puts p.exist? == false
