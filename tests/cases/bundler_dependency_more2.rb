require "bundler/dependency"

dep = Bundler::Dependency.new("demo", ">= 1")
p dep.name
p dep.requirement.to_s
