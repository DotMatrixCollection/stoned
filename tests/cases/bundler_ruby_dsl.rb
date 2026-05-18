$stdout.sync = true

require "bundler/ruby_dsl"

dsl = Bundler::RubyDsl.new
dsl.ruby "3.1.0", "jruby:9.4.0.0"
puts dsl.versions[0]
puts dsl.versions[1]
