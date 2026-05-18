$stdout.sync = true

require "bundler/errors"
puts Bundler::BundlerError.is_a?(Class)
puts Bundler::GemfileNotFound.is_a?(Class)
puts Bundler::GemNotFound.is_a?(Class)
puts Bundler::PermissionError.is_a?(Class)
