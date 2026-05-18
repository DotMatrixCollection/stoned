$stdout.sync = true

require "bundler/specification"

puts Bundler::LazySpecification.is_a?(Class)
puts Bundler::RemoteSpecification.is_a?(Class)
puts Bundler::StubSpecification.is_a?(Class)
puts Bundler::EndpointSpecification.is_a?(Class)
