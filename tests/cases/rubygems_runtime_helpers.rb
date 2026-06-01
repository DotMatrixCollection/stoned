require "rubygems"

p Gem.rubygems_version.to_s
p Gem.ruby_engine
p Gem.user_home.start_with?("/")
p Gem.bindir.end_with?("/bin")
p Gem.suffixes
p Gem.marshal_version
