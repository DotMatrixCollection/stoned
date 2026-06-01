require "rubygems"

p Gem::Platform.new("x86_64-linux").cpu
p Gem::Platform.new("x86_64-linux").os
p Gem::Platform.new("x86_64-linux").to_s
p Gem::Platform.local.is_a?(Gem::Platform)
p Gem.platforms
