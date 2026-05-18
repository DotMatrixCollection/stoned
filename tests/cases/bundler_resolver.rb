$stdout.sync = true

require "bundler/dependency"
require "bundler/index"
require "bundler/resolver"
require "bundler/lazy_specification"

deps = [
  Bundler::Dependency.new("alpha", ">= 0"),
  Bundler::Dependency.new("beta", ">= 0", path: "/tmp/beta")
]

index = Bundler::Index.new
index << Bundler::LazySpecification.new("alpha", "1.2.3")
resolver = Bundler::Resolver.new(deps, index)
specs = resolver.start
puts specs.is_a?(Bundler::SpecSet)
puts specs.length
puts specs[0].full_name
puts specs[1].name
