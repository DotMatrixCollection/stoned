$stdout.sync = true

require "bundler/lockfile_parser"

parser = Bundler::LockfileParser.new(<<~LOCK)
  GEM
    remote: https://rubygems.org/
    specs:
      alpha (1.2.3)

  PATH
    remote: /tmp/demo
    specs:
      beta (0.4.0)

  PLATFORMS
    ruby

  DEPENDENCIES
    alpha
    beta!

  BUNDLED WITH
     2.5.0.stoned
LOCK

puts parser.specs.length
puts parser.specs[0].name
puts parser.specs[0].version
puts parser.specs[1].name
puts parser.specs[1].version
puts parser.specs[0].source.remote
puts parser.specs[1].source.type
puts parser.platforms[0]
puts parser.dependencies[0]
puts parser.dependencies[1]
puts parser.bundler_version
puts parser.sources.length
