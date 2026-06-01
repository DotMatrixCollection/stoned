require "rubygems"

cfg = Gem::ConfigFile.new([])
cfg[:x] = 1
p cfg[:x]
p cfg.backtrace
p cfg.verbose
