require "rubygems"

p Gem.home.length > 0
p Gem.path.include?(Gem.home)
Gem.use_paths("/tmp/stoned_gem_home", ["/tmp/stoned_gem_home", "/tmp/stoned_other_gems"])
p Gem.home
p Gem.path
Gem.clear_paths
p Gem.path.include?(Gem.home)
