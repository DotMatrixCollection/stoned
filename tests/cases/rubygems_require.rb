require "rubygems"

puts Gem::VERSION
puts Gem::RUBYGEMS_VERSION
puts Gem.win_platform?
puts Gem.java_platform?
puts Gem.loaded_specs.is_a?(Hash)
puts Gem::Specification.all.is_a?(Array)

v = Gem::Version.new("1.2.3")
puts v.to_s
puts v.segments.inspect

r = Gem::Requirement.new(">= 1.0")
puts r.to_s
