$stdout.sync = true

require "bundler/source"

puts Bundler::Source::Rubygems.is_a?(Class)
puts Bundler::Source::Path.is_a?(Class)
puts Bundler::Source::Git.is_a?(Class)
