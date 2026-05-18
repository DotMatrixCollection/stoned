$stdout.sync = true

require "bundler/ruby_version"

system_version = Bundler::RubyVersion.system
custom = Bundler::RubyVersion.new(["3.1.0", "3.2.0"], "ruby", "3.2.0", "0")

puts system_version.versions[0] == RUBY_VERSION
puts custom.to_s
puts custom.engine
puts custom.engine_version
puts custom.patchlevel
puts custom.diff(Bundler::RubyVersion.new(["3.1.0", "3.2.0"])).length
puts custom.diff(Bundler::RubyVersion.new(["9.9.9"]))[0]
