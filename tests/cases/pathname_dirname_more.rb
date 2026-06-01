require "pathname"

p = Pathname.new("/tmp/demo/file.txt")
p p.to_s
p p.dirname.to_s
p Pathname.new("/tmp").dirname.to_s
p p.exist?
