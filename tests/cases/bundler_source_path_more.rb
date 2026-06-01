require "bundler/source/path"

src = Bundler::Source::Path.new(path: "/tmp/demo")
p src.path
p src.to_s
