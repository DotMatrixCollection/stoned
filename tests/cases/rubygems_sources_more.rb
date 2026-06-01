require "rubygems"

p Gem::Source.new("https://example.test").uri
p Gem.sources
Gem.sources << "https://mirror.test"
p Gem.sources.include?("https://mirror.test")
