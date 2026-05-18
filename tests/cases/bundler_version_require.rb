$stdout.sync = true

require "bundler/version"
puts Bundler::VERSION
puts(defined?(Bundler::GemfileNotFound) == "constant")
