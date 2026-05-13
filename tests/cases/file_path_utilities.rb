puts File.basename("/foo/bar/baz.rb")
puts File.basename("/foo/bar/baz.rb", ".rb")
puts File.basename("/foo/bar/baz.rb", ".*")
puts File.basename("/foo/bar/")
puts File.basename("baz")
puts File.basename("/")
puts File.basename("")

puts File.dirname("/foo/bar/baz.rb")
puts File.dirname("/foo/bar/")
puts File.dirname("baz")
puts File.dirname("/")

puts File.extname("foo.rb")
puts File.extname("foo.tar.gz")
puts File.extname(".hidden")
puts File.extname("noext")

puts File.join("foo", "bar", "baz")
puts File.join("/foo", "bar")
puts File.join("foo/", "/bar")
puts File.join("/")
puts File.join

p File.split("/foo/bar/baz.rb")
p File.split("baz")
p File.split("/")

puts File.expand_path("/foo/../bar")
puts File.expand_path("/foo/./bar/baz/..")
puts File.expand_path("/already/abs")

puts File.absolute_path("/foo/../bar")
puts File.absolute_path("/foo", "/base")
