$stdout.sync = true

require "bundler/dependency"
require "bundler/endpoint_specification"

deps = [Bundler::Dependency.new("dep", ">= 0")]
spec = Bundler::EndpointSpecification.new("demo", "1.2.3", nil, deps)

puts spec.full_name
puts spec.dependencies.length
puts spec.dependencies[0].name
puts spec.is_a?(Bundler::LazySpecification)
