require "rubygems"

p Gem.datadir("demo").include?("demo")
p Gem.spec_cache_dir.end_with?("/specs")
p Gem.gem_path == Gem.path
