$stdout.sync = true

require "bundler/dependency"

dep = Bundler::Dependency.new("demo", ["~> 1.0", ">= 1.0.5"], groups: [:default, :tools], require: ["demo/core", "demo/extra"], path: "/tmp/demo")
puts dep.name
puts dep.requirement.satisfied_by?(Gem::Version.new("1.2.0"))
puts dep.requirement.satisfied_by?(Gem::Version.new("2.0.0")) == false
puts dep.groups.join(",")
puts dep.autorequire.join(",")
puts dep.source
puts dep.current_platform?
puts dep.should_include?
puts dep.to_lock
