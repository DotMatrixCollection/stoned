require "pathname"

p Pathname.new("/tmp/a").dirname.to_s
p Pathname.new("x/y").dirname.to_s
