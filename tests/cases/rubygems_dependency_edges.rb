require "rubygems"

dep = Gem::Dependency.new("demo", ">= 1.0")
p dep.name
p dep.runtime?
p dep.development?
p dep.requirement.satisfied_by?(Gem::Version.new("1.2.0"))
p dep.to_s
