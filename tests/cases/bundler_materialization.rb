$stdout.sync = true

require "bundler/dependency"
require "bundler/lazy_specification"
require "bundler/materialization"

deps = [Bundler::Dependency.new("alpha", ">= 0")]
specs = [Bundler::LazySpecification.new("alpha", "1.0.0")]
mat = Bundler::Materialization.new(deps, specs)

puts mat.dependencies.length
puts mat.specs.is_a?(Bundler::SpecSet)
puts mat.specs[0].full_name
puts mat.complete?
