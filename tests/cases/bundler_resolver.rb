$stdout.sync = true

require "bundler/dependency"
require "bundler/resolver"
require "bundler/lazy_specification"

deps = [
  Bundler::Dependency.new("alpha", ">= 0"),
  Bundler::Dependency.new("beta", ">= 0", path: "/tmp/beta")
]

resolver = Bundler::Resolver.new(deps)
specs = resolver.start
puts specs.is_a?(Bundler::SpecSet)
puts specs.length
puts specs[0].full_name
puts specs[1].name
