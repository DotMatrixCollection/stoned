$stdout.sync = true

require "bundler/current_ruby"

current = Bundler.current_ruby
puts current.is_a?(Bundler::CurrentRuby)
puts current.ruby_version == RUBY_VERSION
puts current.ruby_platform == RUBY_PLATFORM
puts current.to_s.include?(RUBY_VERSION)
puts current.to_s.include?(RUBY_PLATFORM)
