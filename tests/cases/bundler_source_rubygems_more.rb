require "bundler/source/rubygems"

src = Bundler::Source::Rubygems.new
p src.to_s
p src.remotes
