$stdout.sync = true

require "bundler/source/rubygems"
require "bundler/lazy_specification"
require "bundler/remote_specification"
require "bundler/stub_specification"

source = Bundler::Source::Rubygems.new(remote: "https://rubygems.org")
lazy = Bundler::LazySpecification.new("alpha", "1.2.3", source)
remote = Bundler::RemoteSpecification.new("beta", "2.0.0", source)
stub = Bundler::StubSpecification.new("gamma", "0.4.0", source)

puts lazy.full_name
puts lazy.source.to_s
puts remote.is_a?(Bundler::LazySpecification)
puts remote.to_s
puts stub.is_a?(Bundler::LazySpecification)
puts stub.full_name
