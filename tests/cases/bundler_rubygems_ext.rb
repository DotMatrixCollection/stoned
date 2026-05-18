$stdout.sync = true

require "bundler/rubygems_ext"
puts Bundler::VERSION
puts Bundler.respond_to?(:definition)
puts Bundler.respond_to?(:runtime)
