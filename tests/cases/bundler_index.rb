$stdout.sync = true

require "bundler/dependency"
require "bundler/index"
require "bundler/lazy_specification"

index = Bundler::Index.new
index << Bundler::LazySpecification.new("alpha", "1.2.3")
index << Bundler::LazySpecification.new("alpha", "2.0.0")
index << Bundler::LazySpecification.new("beta", "0.4.0")

dep = Bundler::Dependency.new("alpha", "~> 1.0")
matches = index.search(dep)

puts index.size
puts index.empty? == false
puts matches.length
puts matches[0].full_name
puts index["beta"][0].version
