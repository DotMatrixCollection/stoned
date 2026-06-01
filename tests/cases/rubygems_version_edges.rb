require "rubygems"

v1 = Gem::Version.new("1.2.3")
v2 = Gem::Version.new("1.3.0")
p v1 < v2
p v1.segments
p Gem::Version.new("2.0.0.pre").release.to_s
