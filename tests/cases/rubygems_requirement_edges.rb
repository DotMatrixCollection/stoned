require "rubygems"

req = Gem::Requirement.new("~> 2.1")
p req.satisfied_by?(Gem::Version.new("2.1.5"))
p req.satisfied_by?(Gem::Version.new("3.0.0"))
p req.to_s
p Gem::Requirement.default.satisfied_by?(Gem::Version.new("0.0.1"))
