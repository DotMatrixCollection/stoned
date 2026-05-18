$stdout.sync = true

require "bundler/spec_set"
require "bundler/lockfile_parser"

parser = Bundler::LockfileParser.new(<<~LOCK)
  GEM
    remote: https://rubygems.org/
    specs:
      alpha (1.2.3)
      beta (2.0.0)
LOCK

specs = Bundler::SpecSet.new(parser.specs)
puts specs.length
puts specs.size
puts specs.empty? == false
puts specs[0].name
puts specs[1].version
puts specs.to_a.length
names = []
specs.each { |spec| names << spec.name }
puts names.join(",")
