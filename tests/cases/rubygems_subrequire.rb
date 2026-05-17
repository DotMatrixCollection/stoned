require "rubygems/specification"
require "rubygems/platform"
require "rubygems/name_tuple"
require "rubygems/command"

puts Gem::VERSION
puts Gem.rubygems_version.to_s
puts Gem::Platform.local.is_a?(Gem::Platform)
tuple = Gem::NameTuple.new("rack", "3.0.0")
puts tuple.spec_name
puts Gem::Requirement.default.none?
Gem::Command.build_args = ["--verbose"]
puts Gem::Command.build_args.inspect
